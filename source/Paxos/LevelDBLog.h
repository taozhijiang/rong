/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_LEVELDB_LOG_H__
#define __PAXOS_LEVELDB_LOG_H__

#include <xtra_rhel.h>
#include <mutex>
#include <leveldb/db.h>

#include <system/ConstructException.h>

#include <Paxos/LogIf.h>

namespace rong {

class LevelDBLog : public LogIf {

public:
    explicit LevelDBLog(const std::string& path);
    virtual ~LevelDBLog();


    uint64_t append(uint64_t index, const EntryPtr& newEntry)override;
    uint64_t append(uint64_t index, const std::string& marshalEntry)override;

    EntryPtr entry(uint64_t index) const override;
    std::string entry_marshal(uint64_t index) const override;

    bool entries(uint64_t start, std::vector<EntryPtr>& marshalEntries, uint64_t limit) const override;
    bool entries(uint64_t start, std::vector<std::string>& marshalEntries, uint64_t limit) const override;

    uint64_t start_index() const override {
        return start_index_;
    }

    uint64_t last_index() const override {
        return last_index_;
    }

    void truncate_prefix(uint64_t start_index)override;
    void truncate_suffix(uint64_t last_index)override;


    // Paxos运行状态元数据的持久化，在崩溃重启后修复使用
    int meta_data(LogMeta* meta_data) const override;
    int set_meta_data(const LogMeta& meta) const override;

    uint64_t meta_apply_instance_id() const override;
    int set_meta_apply_instance_id(uint64_t instance_id) const override;

    uint64_t meta_highest_instance_id() const override;
    int set_meta_highest_instance_id(uint64_t instance_id) const override;

protected:

    // 最历史的日志索引，在日志压缩的时候会设置这个值
    uint64_t start_index_;
    uint64_t last_index_;   // 初始化为0，有效值从1开始

    mutable std::mutex  log_mutex_;
    const std::string   log_meta_path_;
    std::unique_ptr<leveldb::DB> log_meta_fp_;
};

} // end namespace rong

#endif // __PAXOS_LEVELDB_LOG_H__
