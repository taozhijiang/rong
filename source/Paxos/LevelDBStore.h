/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_LEVELDB_STORE_H__
#define __PAXOS_LEVELDB_STORE_H__

#include <xtra_rhel.h>
#include <leveldb/db.h>

#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <Paxos/StoreIf.h>
#include <system/ConstructException.h>

#include <Protocol/gen-cpp/Snapshot.pb.h>


// 用来进行实际业务数据存储的LevelDB实例

namespace rong {

class LevelDBStore : public StoreIf {

public:
    explicit LevelDBStore(const std::string& db_path, const std::string& snapshot_path);
    ~LevelDBStore();

    int select_handle(const Client::StateMachineSelectOps::Request& request,
                      Client::StateMachineSelectOps::Response& response) const override;
    int update_handle(const Client::StateMachineUpdateOps::Request& request) const override;

    // 创建和加载状态机快照的时候，会持有锁结构，所以所有的请求将会被阻塞
    // 上层对业务友好的话，可以同步这种快照创建和恢复中的信息

    // 创建状态机快照，先写临时文件，如果成功了再将临时文件重命名为正式文件
    bool create_snapshot(uint64_t last_included_index, uint64_t last_included_term)override;
    bool load_snapshot(std::string& content, uint64_t& last_included_index, uint64_t& last_included_term)override;

    bool apply_snapshot(const Snapshot::SnapshotContent& snapshot)override;

private:

    int get(const std::string& key, std::string& val) const;
    int set(const std::string& key, const std::string& val) const;
    int del(const std::string& key) const;

    int range(const std::string& start, const std::string& end, uint64_t limit,
              std::vector<std::string>& range_store) const;
    int search(const std::string& search_key, uint64_t limit,
               std::vector<std::string>& search_store) const;

protected:
    const std::string kv_path_;
    const std::string snapshot_path_;
    std::unique_ptr<leveldb::DB> kv_fp_;

    mutable boost::shared_mutex kv_lock_;
};

} // namespace rong

#endif // __PAXOS_LEVELDB_STORE_H__
