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

#ifndef ORB_HELPER_H_
#define ORB_HELPER_H_
#include <string>
#include <boost/filesystem/fstream.hpp>
#include <omniORB4/CORBA.h>
#include <boost/thread.hpp>
#include "Logging.h"
#include "IOException.h"

namespace youtils
{
MAKE_EXCEPTION(OrbHelperException, fungi::IOException);
MAKE_EXCEPTION(CouldNotBindObject, OrbHelperException);
MAKE_EXCEPTION(CouldNotGetNameService, OrbHelperException);
MAKE_EXCEPTION(CouldNotNarrowToNameService, OrbHelperException);
MAKE_EXCEPTION(CouldNotNarrowToNamingContext, OrbHelperException);



class OrbHelper
{
public:
    typedef const char* option_t[2];

    typedef option_t options_t[];

    static options_t default_options;

    OrbHelper(const std::string& executable_name,
              const std::string& orb_id = "",
              options_t options = default_options );

    // Has to be enhanced to update it's args when parsing code picks them off.

    ~OrbHelper();

    void
    dump_object_to_file(boost::filesystem::path&,
                        CORBA::Object_ptr p);

    CORBA::Object_ptr
    get_object_from_file(boost::filesystem::path& );


    void
    bindObjectToName(CORBA::Object_ptr objref,
                     const std::string& context_name,
                     const std::string& context_kind,
                     const std::string& object_name,
                     const std::string& object_kind);

    CORBA::Object_ptr
    getObjectReference(const std::string& context_name,
                       const std::string& context_kind,
                       const std::string& object_name,
                       const std::string& object_kind);

    void
    stop();

    CORBA::ORB_var&
    orb()
    {
        return orb_;
    }


private:
    DECLARE_LOGGER("OrbHelper");

    CORBA::ORB_var orb_;

    std::unique_ptr<boost::thread> shutdown_thread_;

    static void
    logFunction(const char* message)
    {
        LOG_INFO(message);
    }

};
}

#endif // ORB_HELPER_H_
