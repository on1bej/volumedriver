// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _RWLOCK_H
#define _RWLOCK_H


#include <pthread.h>
#include "Logging.h"
#include <string>
#include <cerrno>
#include <assert.h>


namespace fungi {

class RWLock

{
public:
    explicit RWLock(const std::string &name);

    RWLock(const RWLock&) = delete;
    RWLock& operator=(const RWLock&) = delete;

    ~RWLock();

    void
    writeLock();

    void
    lock()
    {
        return writeLock();
    }

    bool
    tryWriteLock();

    bool
    try_lock()
    {
        return tryWriteLock();
    }

    void
    readLock();

    void
    lock_shared()
    {
        return readLock();
    }

    bool
    tryReadLock();

    bool
    try_lock_shared()
    {
        return tryReadLock();
    }

    void
    unlock();

    void
    unlock_shared()
    {
        return unlock();
    }

    inline void
    assertWriteLocked() const
    {
        assert(pthread_rwlock_tryrdlock(&rwLock_) == EBUSY);
    }

    inline void
    assertReadLocked() const
    {
        assert(pthread_rwlock_trywrlock(&rwLock_) == EBUSY);
    }

    void
    assertLocked();

private:
    DECLARE_LOGGER("RWLock");

#ifndef NDEBUG
    mutable
#endif
    pthread_rwlock_t rwLock_;
    const std::string name_;
};

class ScopedReadLock
{
public:
    explicit ScopedReadLock(RWLock &rw);

    ScopedReadLock(const ScopedReadLock&) = delete;
    ScopedReadLock& operator=(const ScopedReadLock&) = delete;

    ~ScopedReadLock();

private:
    DECLARE_LOGGER("ScopedReadLock");

    RWLock &rw_;
};

class ScopedWriteLock
{
public:
    explicit ScopedWriteLock(RWLock &rw);

    ScopedWriteLock(const ScopedWriteLock&) = delete;
    ScopedWriteLock& operator=(const ScopedWriteLock&) = delete;


    ~ScopedWriteLock();

private:
    DECLARE_LOGGER("ScopedWriteLock");

    RWLock &rw_;
};

}

#endif // !_RWLOCK_H

// Local Variables: **
// mode: c++ **
// End: **
