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

#include "LockStore.h"

#include <youtils/Logging.h>

namespace backend
{

namespace yt = youtils;

namespace
{

const std::string&
lock_name()
{
    static const std::string s("global_advisory_namespace_lock");
    return s;
}

}

LockStore::LockStore(BackendInterfacePtr bi)
    : bi_(std::move(bi))
{
    if (not bi_->hasExtendedApi())
    {
        LOG_ERROR("Backend type cannot be used as LockStore as it doesn't support the extended API");
        throw BackendNotImplementedException();
    }
}

bool
LockStore::exists()
{
    return bi_->objectExists(lock_name());
}

std::tuple<yt::HeartBeatLock, yt::GlobalLockTag>
LockStore::read()
{
    std::string s;
    const ETag etag(bi_->x_read(s,
                                lock_name(),
                                InsistOnLatestVersion::T).etag_);
    return std::make_tuple(yt::HeartBeatLock(s),
                           static_cast<const yt::GlobalLockTag>(etag));
}

yt::GlobalLockTag
LockStore::write(const yt::HeartBeatLock& lock,
                 const boost::optional<yt::GlobalLockTag>& lock_tag)
{
    std::string s;
    lock.save(s);

    const ETag etag(bi_->x_write(s,
                                 lock_name(),
                                 lock_tag ?
                                 OverwriteObject::T :
                                 OverwriteObject::F,
                                 lock_tag ?
                                 &reinterpret_cast<const ETag&>(*lock_tag) :
                                 nullptr).etag_);
    return reinterpret_cast<const yt::GlobalLockTag&>(etag);
}

const std::string&
LockStore::name() const
{
    return bi_->getNS().str();
}

void
LockStore::erase()
{
    return bi_->remove(lock_name(),
                       ObjectMayNotExist::T);
}

}
