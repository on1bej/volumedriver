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

#include "TestMainHelper.h"
#include <signal.h>
#include <gtest/gtest.h>

namespace youtils
{

void
TestMainHelper::sighand(int)
{
    return;
}

TestMainHelper::TestMainHelper(int argc,
                               char** argv)
    : MainHelper(argc,
                 argv)
{
/*    if(geteuid() == 0)
    {
        std::string msg("These tests cannot be run as root");
        LOG_FATAL(msg);
        throw fungi::IOException(msg.c_str());
    }*/

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, 0);
    sa.sa_handler = &sighand;
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, 0);
}

void
TestMainHelper::log_google_test_help(std::ostream& ostr)
{
    // redirecting cout so google help it goes to the correct stream
    const char* argv[] =
        {
            executable_name_.c_str(),
            "--help"
        };
    int size = 2;

    std::streambuf* old_rdbuf = std::cout.rdbuf();
    try
    {
        std::cout.rdbuf(ostr.rdbuf());
        testing::InitGoogleTest(&size,
                                (char**)argv);
    }
    catch(...)
    {
        std::cout.rdbuf(old_rdbuf);
        throw;
    }
    std::cout.rdbuf(old_rdbuf);
}

void
TestMainHelper::init_google_test()
{

    ArgcArgv args(executable_name_.c_str(),
                  unparsed_options());

    testing::InitGoogleTest(args.argc(), args.argv());
    unparsed_options(*args.argc(), args.argv());
}

int
TestMainHelper::run()
{
    return RUN_ALL_TESTS();
}

}
