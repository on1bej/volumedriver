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

#include "FailOverCacheAcceptor.h"
#include "FailOverCacheProtocol.h"
#include "FailOverCacheStreamers.h"
#include "fungilib/WrapByteArray.h"
#include "fungilib/use_rs.h"

#include <signal.h>
#include <unistd.h>
#include <poll.h>

#include <cerrno>
#include <cstring>

#include <rdma/rdma_cma.h>
#include <rdma/rsocket.h>

#include <youtils/Assert.h>
#include <youtils/ScopeExit.h>

namespace failovercache
{

namespace yt = youtils;
using namespace volumedriver;

FailOverCacheProtocol::FailOverCacheProtocol(std::unique_ptr<fungi::Socket> sock,
                                             fungi::SocketServer& /*parentServer*/,
                                             FailOverCacheAcceptor& fact)
    : sock_(std::move(sock))
    , stream_(*sock_)
    , fact_(fact)
    , use_rs_(sock_->isRdma())
{
    if(pipe(pipes_) != 0)
    {
        stream_.close();
        throw fungi::IOException("could not not open pipe");
    }
    nfds_ = std::max(sock_->fileno(), pipes_[0]) + 1;
    thread_ = new fungi::Thread(*this,
                                true);
};

FailOverCacheProtocol::~FailOverCacheProtocol()
{
    fact_.removeProtocol(this);
    close(pipes_[0]);
    close(pipes_[1]);

    // if(cache_)
    // {
    //     /cache_->unregister_();
    // }

    try
    {
        stream_.close();
        thread_->destroy(); // Yuck: this call does a "delete this" ...
    }
    CATCH_STD_ALL_LOG_IGNORE("Problem shutting down the FailOverCacheProtocol");
}

void FailOverCacheProtocol::start()
{
    thread_->start();
}

void FailOverCacheProtocol::stop()
{
    ssize_t ret;
    ret = write(pipes_[1],"a",1);
    if(ret < 0) {
    	LOG_ERROR("Failed to send stop request to thread: " << strerror(errno));
    }
}

void
FailOverCacheProtocol::run()
{
    try {
        LOG_INFO("Connected");
        while (true)
        {
            int32_t com = 0;

            try
            {
                struct pollfd fds[2];
                fds[0].fd = sock_->fileno();
                fds[1].fd = pipes_[0];

                fds[0].events = POLLIN | POLLPRI;
                fds[1].events = POLLIN | POLLPRI;
                fds[0].revents = POLLIN | POLLPRI;
                fds[1].revents = POLLIN | POLLPRI;

                int res = rs_poll(fds, 2, -1);

                if(res < 0)
                {
                    if(errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        LOG_ERROR("Error in select");
                        throw fungi::IOException("Error in select");
                    }
                }
                else if(fds[1].revents)
                {
                    LOG_INFO("Stop Requested, breaking out main loop");
                    break;
                }
                else
                {
                    stream_ >> fungi::IOBaseStream::cork;
                    stream_ >> com;
                }

            }
            catch (fungi::IOException &e)
            {

                LOG_INFO("Reading command from socked failed, will exit this thread: " << e.what());
                break;
            }

            switch (com)
            {

            case volumedriver::Register:
                LOG_TRACE("Executing Register");
                register_();
                LOG_TRACE("Finished Register");
                break;

            case volumedriver::Unregister:
                LOG_TRACE("Executing Unregister");
                unregister_();
                LOG_TRACE("Finished Unregister");
                break;

            case volumedriver::AddEntries:
                LOG_TRACE("Executing AddEntries");
                addEntries_();
                LOG_TRACE("Finished AddEntries");
                break;
            case volumedriver::GetEntries:
                LOG_TRACE("Executing GetEntries");
                getEntries_();
                LOG_TRACE("Finished GetEntries");
                break;
            case volumedriver::Flush:
                LOG_TRACE("Executing Flush");
                Flush_();
                LOG_TRACE("Finished Flush");
                break;

            case volumedriver::Clear:
                LOG_TRACE("Executing Clear");
                Clear_();
                LOG_TRACE("Finished Clear");
                break;

            case volumedriver::GetSCORange:
                LOG_TRACE("Executing GetSCORange");
                getSCORange_();
                LOG_TRACE("Finished GetSCORange");
                break;

            case volumedriver::GetSCO:
                LOG_TRACE("Executing GetSCO");
                getSCO_();
                LOG_TRACE("Finished GetSCO");
                break;
            case volumedriver::RemoveUpTo:
                LOG_TRACE("Executing RemoveUpTo");
                removeUpTo_();
                LOG_TRACE("Finished RemoveUpTo");

                break;
            default:
                LOG_ERROR("DEFAULT BRANCH IN SWITCH...");
                throw fungi :: IOException("no valid command");
            }
        }
    }
    catch (fungi::IOException &e)
    {
        returnNotOk();
        LOG_ERROR("Exception in thread: " << e.what());
    }
    if(cache_)
    {
        LOG_INFO("Exiting cache server for namespace: " << cache_->getNamespace());
    }
    else
    {
        LOG_INFO("Exiting cache server before even registering (cache_ == 0), probably framework ping");
    }
}

void
FailOverCacheProtocol::register_()
{
    volumedriver::CommandData<volumedriver::Register> data;
    stream_ >> data;

    LOG_INFO("Registering namespace " << data.ns_);
    cache_ = fact_.lookup(data);
    if(not cache_)
    {
        returnNotOk();
    }
    else
    {
        VERIFY(cache_->registered());
        returnOk();
    }
}

void
FailOverCacheProtocol::unregister_()
{
    VERIFY(cache_);
    LOG_INFO("Unregistering namespace " << cache_->getNamespace());
    try
    {
        // cache_->unregister_();
        fact_.remove(*cache_);
        cache_ = nullptr;
        stream_ << fungi::IOBaseStream::cork;
        OUT_ENUM(stream_,volumedriver::Ok);
        stream_ << fungi::IOBaseStream::uncork;
    }
    catch(...)
    {
        stream_ << fungi::IOBaseStream::cork;
        OUT_ENUM(stream_, volumedriver::NotOk);
        stream_ << fungi::IOBaseStream::uncork;
    }

}

void
FailOverCacheProtocol::addEntries_()
{
    VERIFY(cache_);

    volumedriver::CommandData<volumedriver::AddEntries> data; // default constructor, as we are streaming in
    stream_ >> data;
    for(std::vector<volumedriver::FailOverCacheEntry>::const_iterator it = data.entries_.begin();
        it != data.entries_.end();
        ++it)
    {
        TODO("ArneT: funky cast...");
        cache_->addEntry(it->cli_,
                         it->lba_,
                         (byte*) it->buffer_,
                         it->size_);
    }
    returnOk();
}
void
FailOverCacheProtocol::returnOk()
{
    stream_ << fungi::IOBaseStream::cork;
    OUT_ENUM(stream_, volumedriver::Ok);
    stream_ << fungi::IOBaseStream::uncork;
}

void
FailOverCacheProtocol::returnNotOk()
{
    stream_ << fungi::IOBaseStream::cork;
    OUT_ENUM(stream_, volumedriver::NotOk);
    stream_ << fungi::IOBaseStream::uncork;
}

void
FailOverCacheProtocol::Flush_()
{
    VERIFY(cache_);
    LOG_TRACE("Flushing for namespace " <<  cache_->getNamespace());
    cache_->Flush();
    returnOk();
}

void
FailOverCacheProtocol::Clear_()
{
    VERIFY(cache_);
    LOG_INFO("Clearing for namespace " << cache_->getNamespace());
    cache_->Clear();
    returnOk();
}

void
FailOverCacheProtocol::processFailOverCacheEntry(volumedriver::ClusterLocation cli,
                                         int64_t lba,
                                         const byte* buf,
                                         int64_t size)
{
    // Y42 better logging here
    LOG_TRACE("Sending Entry for lba " << lba );
    if (use_rs_) // make small but finished packets with RDMA
    {
        stream_ << fungi::IOBaseStream::cork;
    }
    stream_ << cli;
    stream_ << lba;
    const fungi::WrapByteArray a((byte*)buf, (int32_t)size);
    stream_ << a;
    if (use_rs_) // make small but finished packets with RDMA
    {
        stream_ << fungi::IOBaseStream::uncork;
    }
}

void
FailOverCacheProtocol::removeUpTo_()
{
    VERIFY(cache_);
    LOG_INFO("Namespace " << cache_->getNamespace());


    volumedriver::SCO sconame;

    stream_ >> sconame;
    try
    {
        cache_->removeUpTo(sconame);
        returnOk();
    }
    catch(std::exception& e)
    {
        LOG_ERROR("Exception caught, ignoring" << e.what());
        returnNotOk();
    }

    catch(...)
    {
        LOG_ERROR("Unknown exception caught, ignoring");
        returnNotOk();
    }
}

void
FailOverCacheProtocol::getEntries_()
{
    VERIFY(cache_);
    LOG_INFO("Namespace " <<  cache_->getNamespace());

    auto on_exit(yt::make_scope_exit([&]
    {
        try
        {
            volumedriver::ClusterLocation end_cli;
            stream_ << end_cli;
            stream_ << fungi::IOBaseStream::uncork;
        }
        CATCH_STD_ALL_LOGLEVEL_IGNORE("Could not send eof data to FOC at the other end",
                                      WARN);
    }));

    stream_ << fungi::IOBaseStream::cork;
    cache_->getEntries(this);
}

void
FailOverCacheProtocol::getSCO_()
{
    VERIFY(cache_);
    LOG_INFO("Namespace" << cache_->getNamespace());
    SCO scoName;

    stream_ >> scoName;

    auto on_exit(yt::make_scope_exit([&]
    {
        try
        {
            volumedriver::ClusterLocation end_cli;
            stream_ << end_cli;
            stream_ << fungi::IOBaseStream::uncork;
        }
        CATCH_STD_ALL_LOGLEVEL_IGNORE("Could not send eof data to FOC at the other end",
                                      WARN);
    }));

    stream_ << fungi::IOBaseStream::cork;
    cache_->getSCO(scoName,
                    this);
}

void
FailOverCacheProtocol::getSCORange_()
{
    VERIFY(cache_);
    LOG_INFO("Namespace" << cache_->getNamespace());
    SCO oldest;
    SCO youngest;
    cache_->getSCORange(oldest,
                        youngest);

    stream_ << fungi::IOBaseStream::cork;
    stream_ << oldest;
    stream_ << youngest;

    stream_ << fungi::IOBaseStream::uncork;
}

void
FailOverCacheProtocol::processFailOverCacheSCO(volumedriver::ClusterLocation cli,
                                               int64_t lba,
                                               const byte* buf,
                                               int64_t size)
{
    // Y42 do we want to use the same process function as above.
    stream_ << cli;
    stream_ << lba;
    const fungi::WrapByteArray a((byte*)buf, (int32_t)size);
    stream_ << a;
}

}

// Local Variables: **
// mode: c++ **
// End: **
