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

#ifndef MDS_ROCKS_CONFIG_H_
#define MDS_ROCKS_CONFIG_H_

#include <boost/optional.hpp>

#include <rocksdb/options.h>

#include <youtils/BooleanEnum.h>
#include <youtils/OurStrongTypedef.h>

OUR_STRONG_ARITHMETIC_TYPEDEF(size_t,
                              WriteCacheSize,
                              metadata_server);

OUR_STRONG_ARITHMETIC_TYPEDEF(size_t,
                              ReadCacheSize,
                              metadata_server);

OUR_STRONG_ARITHMETIC_TYPEDEF(size_t,
                              DbThreads,
                              metadata_server);

namespace metadata_server
{

BOOLEAN_ENUM(EnableWal);
BOOLEAN_ENUM(DataSync);

struct RocksConfig
{
    enum class CompactionStyle
    {
        Level,
        Universal,
        Fifo
            // None is not exposed for now while we don't invoke compaction ourselves
    };

    boost::optional<DbThreads> db_threads;
    boost::optional<WriteCacheSize> write_cache_size;
    boost::optional<ReadCacheSize> read_cache_size;
    boost::optional<EnableWal> enable_wal;
    boost::optional<DataSync> data_sync;
    boost::optional<int> max_write_buffer_number;
    boost::optional<int> min_write_buffer_number_to_merge;
    boost::optional<int> num_levels;
    boost::optional<int> level0_file_num_compaction_trigger;
    boost::optional<int> level0_slowdown_writes_trigger;
    boost::optional<int> level0_stop_writes_trigger;
    // max_mem_compaction_level?
    boost::optional<uint64_t> target_file_size_base;
    // target_file_size_multiplier?
    boost::optional<uint64_t> max_bytes_for_level_base;
    // level_compaction_dynamic_level_bytes:
    // not to be reconfigured for existing DBs so we leave it alone (false)
    // max_bytes_for_level_multiplier?
    // max_bytes_for_level_multiplier_additional?
    // expanded_compaction_factor?
    // source_compaction_factor?
    // max_grandparent_overlap_factor?
    // soft_rate_limit?
    // hard_rate_limit?
    // rate_limit_delay_max_milliseconds?
    // arena_block_size?
    // disable_auto_compactions? not while we don't trigger them manually ...
    // purge_redundant_kvs_while_flush?
    boost::optional<CompactionStyle> compaction_style;
    // verify_checksums_in_compaction?
    // filter_deletes? we don't do these without TRIM support

    RocksConfig() = default;

    ~RocksConfig() = default;

    RocksConfig(const RocksConfig&) = default;

    RocksConfig&
    operator=(const RocksConfig&) = default;

    RocksConfig(RocksConfig&&) = default;

    RocksConfig&
    operator=(RocksConfig&&) = default;

    bool
    operator==(const RocksConfig&) const;

    bool
    operator!=(const RocksConfig& other) const
    {
        return not operator==(other);
    }

    rocksdb::DBOptions
    db_options(const std::string& id) const;

    rocksdb::ColumnFamilyOptions
    column_family_options() const;

    rocksdb::ReadOptions
    read_options() const;

    rocksdb::WriteOptions
    write_options() const;
};

std::ostream&
operator<<(std::ostream&,
           const RocksConfig::CompactionStyle);

std::istream&
operator>>(std::istream&,
           RocksConfig::CompactionStyle&);

std::ostream&
operator<<(std::ostream&,
           const RocksConfig&);
}

#endif // !MDS_ROCKSCONFIG_H_
