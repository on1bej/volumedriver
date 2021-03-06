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

#ifndef CACHED_SCO_H_
#define CACHED_SCO_H_

#include "SCO.h"
#include "Types.h"

#include <boost/filesystem.hpp>
#include <boost/interprocess/detail/atomic.hpp>
#include <boost/intrusive/set.hpp>
#include <youtils/FileDescriptor.h>
#include <youtils/SpinLock.h>

namespace volumedriver
{

namespace bi = boost::intrusive;
using youtils::FDMode;

class SCOCacheMountPoint;
class SCOCacheNamespace;

// Z42: redundant to the one in SCOCacheMountPoint.h - clean up!
typedef boost::intrusive_ptr<SCOCacheMountPoint> SCOCacheMountPointPtr;

class CachedSCO
    : public bi::set_base_hook<bi::link_mode<bi::auto_unlink> >
{
    friend class SCOCacheMountPoint;
    friend class CachedSCOTest;
    friend class SCOCache;

public:
    ~CachedSCO();

    CachedSCO(const CachedSCO&) = delete;
    CachedSCO& operator=(const CachedSCO&) = delete;

    SCOCacheNamespace*
    getNamespace() const;

    SCO
    getSCO() const;

    SCOCacheMountPointPtr
    getMountPoint();

    uint64_t
    getSize() const;

    uint64_t
    getRealSize() const;

    float
    getXVal() const;

    void
    setXVal(float);

    OpenSCOPtr
    open(FDMode mode);

    void
    incRefCount(uint32_t num);

    uint32_t
    use_count();

    void
    remove();

    const fs::path&
    path() const;

private:
    DECLARE_LOGGER("CachedSCO");

    const fs::path path_;
    SCOCacheNamespace* const nspace_;
    SCO scoName_;
    SCOCacheMountPointPtr mntPoint_;
    uint64_t size_;
    float xVal_;
    bool disposable_;
    bool unlink_on_destruction_;
    volatile boost::uint32_t refcnt_;

    // protect the following four from being called arbitrarily in the code:
    //
    // - scans an existing SCO, only allowed from SCOCacheMountPoint
    CachedSCO(SCOCacheNamespace* nspace,
              SCO scoName,
              SCOCacheMountPointPtr mntPoint,
              const fs::path& path);

    // - create a new SCO, only allowed from SCOCacheMountPoint
    CachedSCO(SCOCacheNamespace* nspace,
              SCO scoName,
              SCOCacheMountPointPtr mntPoint,
              uint64_t maxSize,
              float xval);

    // the following 2 need to be protected against races by routing the
    // calls through SCOCache and relying on the locking there. Possible races:
    //
    // - the cleaner (read, periodic action thread) vs. datastore (write,
    //   threadpool thread)
    // - datastore (read, volume thread) vs. datastore (write, threadpool thread)
    //
    // The latter one is actually already prevented by the datastore locking,
    // so isDisposable() could be made public, but this is obviously too brittle.
    void
    setDisposable();

    bool
    isDisposable() const;

    void
    checkMountPointOnline_() const;

    friend void
    intrusive_ptr_add_ref(CachedSCO *);

    friend void
    intrusive_ptr_release(CachedSCO *);
};

}

#endif // !CACHED_SCO_H_

// Local Variables: **
// mode: c++ **
// End: **
