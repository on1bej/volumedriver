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

#ifndef CHOOSER_H_
#define CHOOSER_H_
#include "Assert.h"
#include <boost/random/discrete_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>

namespace youtils
{
template<typename T,
         typename G = boost::random::mt19937>
class Chooser
{
public:
    Chooser(const std::vector<std::pair<T, double> >& choices)
        : dist_(get_odds(choices))
        , choices_(get_choices(choices))
    {
    }

    Chooser(const std::vector<std::pair<T, unsigned> >& choices)
        : dist_(get_weights(choices))
        , choices_(get_choices(choices))
    {
    }

    const T&
    operator()()
    {
        return choices_[dist_(gen_)];
    }

    G gen_;
    DECLARE_LOGGER("Chooser");

private:

    // A range adaptor might be a cool exercise here.
    std::vector<double>
    get_odds(const std::vector<std::pair<T, double> >& choices)
    {
        std::vector<double> res;
        res.reserve(choices.size());
        double acc = 0;

        for(size_t i = 0; i < choices.size(); ++i)
        {
            double val = choices[i].second;
            res.push_back(val);
            acc += val;

        }
        VERIFY(acc == 1.0);
        return res;
    }

    std::vector<unsigned>
    get_weights(const std::vector<std::pair<T, unsigned> >& choices)
    {
        std::vector<unsigned> res;
        res.reserve(choices.size());

        for(size_t i = 0; i < choices.size(); ++i)
        {
            res.push_back(choices[i].second);
        }
        return res;
    }

    template <typename d>
    std::vector<T>
    get_choices(const std::vector<std::pair<T, d> >& choices)
    {
        std::vector<T> res;
        res.reserve(choices.size());
        for(size_t i = 0; i < choices.size(); ++i)
        {
            res.push_back(choices[i].first);
        }
        return res;
    }
    boost::random::discrete_distribution<> dist_;
    std::vector<T> choices_;
};

}

#endif // CHOOSER_H_

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
