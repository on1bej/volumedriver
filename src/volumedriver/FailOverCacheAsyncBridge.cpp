// Copyright 2015 iNuron NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "FailOverCacheAsyncBridge.h"
#include "Volume.h"

#include <algorithm>
#include <cerrno>

namespace volumedriver
{

#define LOCK()                                                          \
    boost::lock_guard<decltype(mutex_)> lg__(mutex_)

#define LOCK_NEW_ONES()                                                 \
    boost::lock_guard<decltype(new_ones_mutex_)> nolg__(new_ones_mutex_)

const boost::chrono::seconds
FailOverCacheAsyncBridge::timeout_(1);

// Passing LBASize and ClusterMultiplier here is ugly as sin as the underlying
// FailOverCacheProxy also tracks that information. However, the locking of this
// thing is rather convoluted and needs adaptations if we want to get these settings
// out of the cache_ member.
TODO("AR: get rid of LBASize and ClusterMultiplier");
FailOverCacheAsyncBridge::FailOverCacheAsyncBridge(const LBASize lba_size,
                                                   const ClusterMultiplier cluster_multiplier,
                                                   const size_t max_entries,
                                                   const std::atomic<unsigned>& write_trigger)
    : FailOverCacheClientInterface(max_entries)
    , lba_size_(lba_size)
    , cluster_multiplier_(cluster_multiplier)
    , newData(cluster_size_() * max_entries)
    , oldData(cluster_size_() * max_entries)
    , write_trigger_(write_trigger)
    , thread_(nullptr)
    , stop_(true)
    , throttling(false)
{
}

void
FailOverCacheAsyncBridge::init_cache_()
{
    //PRECONDITION: thread_ = 0, oldOnes = 0, newones = 0
    // Y42 why not assert for these preconditions??
    // Y42 also a throw might put a monkey wrench into these so called preconditions
    if(cache_)
    {
        thread_ = new fungi::Thread(*this,false);
        stop_ = false;
        thread_->StartWithoutCleanup();
    }
}

void
FailOverCacheAsyncBridge::initialize(DegradedFun fun)
{
    LOCK();

    //SHOULD NEVER BE CALLED TWICE as it breaks initcache's preconditions
    ASSERT(not degraded_fun_);
    degraded_fun_ = std::move(fun);

    init_cache_();
}

const char*
FailOverCacheAsyncBridge::getName() const
{
    return "FailOverCacheAsyncBridge";
}

bool
FailOverCacheAsyncBridge::backup()
{
    return cache_ != nullptr;
}

void
FailOverCacheAsyncBridge::newCache(std::unique_ptr<FailOverCacheProxy> cache)
{
    LOG_INFO("newCache");

    if (cache)
    {
        VERIFY(cache->lba_size() == lba_size_);
        VERIFY(cache->cluster_multiplier() == cluster_multiplier_);
    }

    {
        LOCK();
        stop_ = true;
        condvar_.notify_one();
    }
    if(cache_)
    {
        cache_->delete_failover_dir();

        thread_->join();
        thread_->destroy();
        thread_ = nullptr;

        {
            LOCK();
            LOCK_NEW_ONES();
            newOnes.clear();
            oldOnes.clear();
        }
    }

    cache_ = std::move(cache);

    init_cache_();
}

// Called from Volume::destroy
void
FailOverCacheAsyncBridge::destroy(SyncFailOverToBackend sync)
{

    {
        LOCK();
        stop_ = true;
        condvar_.notify_one();
    }
    if (cache_)
    {

        thread_->join();
        thread_->destroy();
        thread_ = 0;
        // VOLUME SHOULD HAVE BEEN SYNCED AND DETACHED
        //  VERIFY(oldOnes.empty() and
        // newOnes.empty());
        {
            LOCK_NEW_ONES();
            if (not oldOnes.empty())
            {
                if(T(sync))
                {
                    try
                    {
                        cache_->addEntries(oldOnes);
                    }
                    // Z42: std::exception?
                    catch(fungi::IOException& e)
                    {
                        LOG_ERROR("problem adding entries " << e.what() << ", ignoring");
                    }

                }
                oldOnes.clear();
            }
        }

        {
            LOCK_NEW_ONES();

            if(not newOnes.empty())
            {
                if(T(sync))
                {
                    try
                    {
                        cache_->addEntries(newOnes);
                    }
                    // Z42: std::exception?
                    catch(fungi::IOException& e)
                    {
                        LOG_ERROR("problem adding entries " << e.what() << ", ignoring");
                    }

                }
                newOnes.clear();

            }
        }
        {
            LOCK();
            cache_ = nullptr;
        }

    }
}

void
FailOverCacheAsyncBridge::setRequestTimeout(const boost::chrono::seconds seconds)
{
    LOCK();

    if(cache_)
    {
        cache_->setRequestTimeout(seconds);
    }
}

void
FailOverCacheAsyncBridge::run()
{
    boost::unique_lock<decltype(mutex_)> unique_lock(mutex_);

    while (not stop_)
    {
        try
        {
            if (not oldOnes.empty())
            {
                LOG_DEBUG("Writing " << oldOnes.size() << " entries to the failover cache");
                cache_->addEntries(oldOnes);
                oldOnes.clear();
                LOG_DEBUG("Written");
            }
            else
            {
                // Y42 The machine that goes ping
                cache_->flush();
            }

            condvar_.wait_for(unique_lock,
                              timeout_);

            if (oldOnes.empty())
            {
                //we get here by a timeout -> time to swap
                //no risk for deadlock as addEntry only does a tryLock on mutex_
                LOCK_NEW_ONES();

                std::swap(newOnes, oldOnes);
                std::swap(newData, oldData);
            }
        }
        catch (std::exception& e)
        {
            LOG_ERROR("Exception in failover thread: " << e.what());
            if(degraded_fun_)
            {
                degraded_fun_();
            }

            LOCK_NEW_ONES();

            stop_ = true;
            newOnes.clear();
            oldOnes.clear();

            // This thread cleans up itself when it is detached
            thread_->detach();
            thread_ = nullptr;
            cache_ = nullptr;
        }
        // Z42: catch (...) ?
    }
}

// Y42: This needs to be fixed!!!
void
FailOverCacheAsyncBridge::setThrottling(bool v)
{
    if(v and (not throttling))
    {
        LOG_DEBUG("Slowing down write operations because the FailOverCache is not able to sustain the write speed");
    }
    if((not v) and throttling)
    {
        LOG_DEBUG("Resuming write operations since the FailOverCache is able to sustain the write speed");
    }
    throttling = v;
}

void
FailOverCacheAsyncBridge::addEntry(ClusterLocation loc,
                                   uint64_t lba,
                                   const uint8_t* buf,
                                   size_t /* bufsize */)
{
    uint8_t* ptr = newData.data() + (newOnes.size() * cluster_size_());
    memcpy(ptr, buf, cluster_size_());
    newOnes.emplace_back(loc, lba, ptr, cluster_size_());
}

bool
FailOverCacheAsyncBridge::addEntries(const std::vector<ClusterLocation>& locs,
                                     size_t num_locs,
                                     uint64_t start_address,
                                     const uint8_t* data)
{
    VERIFY(num_locs <= max_entries());

    LOCK_NEW_ONES();

    // Needs improvement!!!
    if (stop_)
    {
        return true;
    }

    // Just see if there is enough space for the whole batch
    setThrottling(newOnes.size() + num_locs > max_entries());
    if (not throttling)
    {
        // Otherwise work the batch
        for (size_t i = 0; i < num_locs; ++i)
        {
            addEntry(locs[i],
                     start_address + i * cluster_multiplier_,
                     data + i * cluster_size_(),
                     cluster_size_());
        }

    }

    maybe_swap_();

    return not throttling;
}

void
FailOverCacheAsyncBridge::maybe_swap_()
{
    if (newOnes.size() >= write_trigger_)
    {
        //must be trylock otherwise risk for deadlock
        boost::unique_lock<decltype(mutex_)> u(mutex_,
                                               boost::try_to_lock);
        if (u)
        {
            if (oldOnes.empty())
            {
                std::swap(newOnes, oldOnes);
                std::swap(newData, oldData);
                condvar_.notify_one();
            }
        }
    }
}

void FailOverCacheAsyncBridge::Flush()
{
    LOCK();
    flush_();
}

void
FailOverCacheAsyncBridge::flush_()
{
    if(cache_)
    {
        LOCK_NEW_ONES();

        LOG_DEBUG("oldOnes: " << oldOnes.size() << " ,newOnes: " << newOnes.size());

        oldOnes.reserve(oldOnes.size() + newOnes.size());
        for (auto&& v : newOnes)
        {
            oldOnes.emplace_back(std::move(v));
        }

        newOnes.clear();

        try
        {
            cache_->addEntries(std::move(oldOnes));
            oldOnes.clear();
            cache_->flush();
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR("Failed to flush: " << EWHAT);
                if (degraded_fun_)
                {
                    degraded_fun_();
                }
            });
    }
}

void
FailOverCacheAsyncBridge::removeUpTo(const SCO& sconame)
{
    LOCK();

    if(cache_)
    {
        cache_->removeUpTo(sconame);
    }
}

void
FailOverCacheAsyncBridge::Clear()
{
    LOCK();

    if(cache_)
    {
        cache_->clear();
    }
}

uint64_t
FailOverCacheAsyncBridge::getSCOFromFailOver(SCO sconame,
                                             SCOProcessorFun processor)
{
    LOCK();

    flush_(); // Z42: too much overhead?

    if (cache_)
    {
        return cache_->getSCOFromFailOver(sconame,
                                          processor);
    }
    else
    {
        throw FailOverCacheNotConfiguredException();
    }
}

FailOverCacheMode
FailOverCacheAsyncBridge::mode() const
{
    return FailOverCacheMode::Asynchronous;
}

}

// Local Variables: **
// mode: c++ **
// End: **
