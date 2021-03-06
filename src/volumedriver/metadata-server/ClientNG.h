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

#ifndef METADATA_SERVER_CLIENT_H_
#define METADATA_SERVER_CLIENT_H_

#include "Interface.h"
#include "Protocol.h"

#include <chrono>
#include <memory>

#include <boost/thread.hpp>

#include <capnp/message.h>

#include <youtils/LocORemClient.h>
#include <youtils/SharedMemoryRegion.h>

namespace volumedriver
{

class MDSNodeConfig;

}

namespace metadata_server
{

class TableHandle;

class ClientNG
    : public DataBaseInterface
    , public std::enable_shared_from_this<ClientNG>
{
public:
    using Ptr = std::shared_ptr<ClientNG>;

    static Ptr
    create(const volumedriver::MDSNodeConfig& cfg,
           size_t shmem_size = 8ULL << 10,
           const boost::optional<std::chrono::seconds>& timeout = boost::none,
           ForceRemote force_remote = ForceRemote::F);

    ~ClientNG();

    ClientNG(const ClientNG&) = delete;

    ClientNG&
    operator=(const ClientNG&) = delete;

    virtual TableInterfacePtr
    open(const std::string& nspace) override final;

    virtual void
    drop(const std::string& nspace) override final;

    virtual std::vector<std::string>
    list_namespaces() override final;

    void
    ping(const std::vector<uint8_t>& out,
         std::vector<uint8_t>& in);

    bool
    is_local() const
    {
        return client_.is_local();
    }

    enum class Direction
    {
        Out,
        In
    };

    template<enum Direction D>
    struct Counters
    {
        uint64_t messages = 0;
        uint64_t data_bytes = 0; // excluding headers!
        uint64_t data_bytes_sqsum = 0;
        uint64_t shmem_overruns = 0;
    };

    using OutCounters = Counters<Direction::Out>;
    using InCounters = Counters<Direction::In>;

    void
    counters(OutCounters& out,
             InCounters& in) const
    {
        out = out_counters_;
        in = in_counters_;
    }

private:
    DECLARE_LOGGER("MetaDataServerClientNG");

    friend class TableHandle;

    boost::mutex lock_;
    youtils::LocORemClient client_;
    std::unique_ptr<youtils::SharedMemoryRegion> mr_;
    const boost::optional<std::chrono::seconds> timeout_;

    OutCounters out_counters_;
    InCounters in_counters_;

    ClientNG(const volumedriver::MDSNodeConfig& cfg,
             size_t shmem_size,
             const boost::optional<std::chrono::seconds>& timeout,
             ForceRemote force_remote);

    template<enum metadata_server_protocol::RequestHeader::Type r,
             typename Build>
    size_t
    send_shmem_(metadata_server_protocol::Tag tag,
                Build&& build);

    template<enum metadata_server_protocol::RequestHeader::Type r,
             typename Build>
    size_t
    send_inband_(metadata_server_protocol::Tag tag,
                 Build&& build);

    template<enum metadata_server_protocol::RequestHeader::Type r,
             typename Build,
             typename Read>
    void
    interact_(Build&&,
              Read&&);

    template<enum metadata_server_protocol::RequestHeader::Type r,
             typename Read>
    void
    recv_(metadata_server_protocol::Tag tag,
          size_t txsize,
          Read&&);

    template<enum metadata_server_protocol::RequestHeader::Type T,
             typename Read>
    void
    handle_response_(const metadata_server_protocol::ResponseHeader&,
                     capnp::MessageReader&,
                     Read&&);

    void
    prepare_shmem_();

    bool
    use_shmem_() const
    {
        return mr_ != nullptr and is_local();
    }
};

}

#endif // !METADATA_SERVER_CLIENT_H_
