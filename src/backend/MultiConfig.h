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

#ifndef BACKEND_MULTI_CONFIG_H_
#define BACKEND_MULTI_CONFIG_H_

#include "BackendConfig.h"

#include <youtils/IOException.h>
#include <youtils/Logging.h>

namespace backend
{

MAKE_EXCEPTION(DifferentTypesInMultiException, fungi::IOException);

class MultiConfig
    : public BackendConfig
{
    DECLARE_LOGGER("MultiConfig");

public:
    MultiConfig(const std::vector<std::unique_ptr<BackendConfig>>& configs)
        : BackendConfig(BackendType::MULTI)
    {
        // SOME VALUE TO STOP THE COMPILER FROM COMPLAINING
        BackendType backend_type = BackendType::MULTI;

        for(size_t i = 0; i < configs.size(); ++i)
        {
            if(i == 0)
            {
                backend_type = configs[i]->backend_type.value();
            }
            else
            {
                if(backend_type != configs[i]->backend_type.value())
                {
                    LOG_ERROR("Multi has different types, " << backend_type
                              << " and " << configs[i]->backend_type.value());
                    throw DifferentTypesInMultiException("Different Types in Multi");
                }

            }

            configs_.push_back(configs[i]->clone());
        }
    }

    MultiConfig(const boost::property_tree::ptree& pt)
        : BackendConfig(BackendType::MULTI)
    {
        const boost::property_tree::ptree& backend_connection_manager = pt.get_child("backend_connection_manager");
        int index = 0;
        // SOME VALUE TO STOP THE COMPILER FROM COMPLAINING
        BackendType backend_type = BackendType::MULTI;
        while(true)
        {
            std::stringstream ss;
            ss << index;

            boost::optional<const boost::property_tree::ptree&> config =
                backend_connection_manager.get_child_optional(ss.str().c_str());
            if(config)
            {
                boost::property_tree::ptree temp;
                temp.put_child("backend_connection_manager",
                               *config);
                std::unique_ptr<BackendConfig> config(makeBackendConfig(temp));
                if(index == 0)
                {
                    backend_type = config->backend_type.value();
                }
                else
                {
                    if(backend_type != config->backend_type.value())
                    {
                        LOG_ERROR("Multi has different types, "
                                  << backend_type << " and " << config->backend_type.value());
                        throw DifferentTypesInMultiException("Different Types in Multi");
                    }
                }

                configs_.push_back(std::move(config));
                ++index;
            }
            else
            {
                break;
            }
        }
    }


    MultiConfig(const MultiConfig&) = delete;
    MultiConfig&
    operator=(const MultiConfig&) = delete;

    virtual const char*
    name() const override final
    {
        static const char* n = "MULTI";
        return n;
    }


    virtual StrictConsistency
    strict_consistency() const override final
    {
        if (not configs_.empty())
        {

            return configs_.front()->strict_consistency();
        }
        else
        {
            return StrictConsistency::T;
        }

    }



    virtual std::unique_ptr<BackendConfig>
    clone() const override final
    {
        std::unique_ptr<BackendConfig>
            bc(new MultiConfig(configs_));

        return bc;
    }

    virtual void
    persist_internal(boost::property_tree::ptree& pt,
                     const ReportDefault reportDefault) const override final
    {
        putBackendType(pt,
                       reportDefault);
        boost::property_tree::ptree& backend_tree =
            pt.get_child(std::string(initialized_params::backend_connection_manager_name));

        for(unsigned i = 0; i < configs_.size(); ++i)
        {
            boost::property_tree::ptree pt_tmp;
            configs_[i]->persist_internal(pt_tmp,
                                         reportDefault);
            boost::property_tree::ptree& pt_child =
                pt_tmp.get_child(initialized_params::backend_connection_manager_name);

            backend_tree.put_child(boost::lexical_cast<std::string>(i),
                                   pt_child);
        }
    }

    virtual void
    update_internal(const boost::property_tree::ptree&,
                    youtils::UpdateReport&) override final
    {}

    virtual bool
    operator==(const BackendConfig& other) const override final
    {
        return other.backend_type.value() == BackendType::MULTI and
            all_configs_equal(static_cast<const MultiConfig&>(other));
    }

    virtual bool
    checkConfig_internal(const boost::property_tree::ptree&,
                         youtils::ConfigurationReport&) const override final
    {
        return true;
    };

    std::vector<std::unique_ptr<BackendConfig> > configs_;

private:
    bool
    all_configs_equal(const MultiConfig& other) const
    {
       if(configs_.size() != other.configs_.size())
        {
            return false;
        }

        for(size_t i = 0; i < configs_.size(); ++i)
        {
            if(not( *configs_[i] == *other.configs_[i]))
            {
                return false;
            }
        }
        return true;
    }
};

}

#endif // !BACKEND_MULTI_CONFIG_H_
