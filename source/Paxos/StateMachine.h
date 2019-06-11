/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_STATE_MACHINE__
#define __PAXOS_STATE_MACHINE__

#include <xtra_rhel.h>

#include <mutex>
#include <string>
#include <thread>

#include <Paxos/LogIf.h>
#include <Paxos/StoreIf.h>

// 简易的KV存储支撑

namespace rong {


enum class SnapshotProgress : uint8_t {
    kBegin      = 1,
    kProcessing = 2,
    kDone       = 3,
};

class RaftConsensus;


class StateMachine {

    __noncopyable__(StateMachine)

public:

    StateMachine(PaxosConsensus& paxos_consensus,
                 std::unique_ptr<LogIf>& log_meta, std::unique_ptr<StoreIf>& kv_store);
    ~StateMachine() = default;

    bool init();

    void notify_state_machine() { apply_notify_.notify_all(); }
    void state_machine_loop();

    // 本地快照的创建和加载
    bool create_snapshot(uint64_t& last_included_index, uint64_t& last_included_term);
    bool load_snapshot(std::string& content, uint64_t& last_included_index, uint64_t& last_included_term);
    bool apply_snapshot(const Snapshot::SnapshotContent& snapshot);

    uint64_t apply_instance_id() const { return apply_instance_id_; }
    void set_apply_instance_id(uint64_t instance_id) { apply_instance_id_ = instance_id; }

    bool fetch_response_msg(uint64_t instance_id, std::string& content);

private:

    int do_apply(LogIf::EntryPtr entry, std::string& content_out);

    PaxosConsensus&             paxos_consensus_;
    std::unique_ptr<LogIf>&     log_meta_;
    std::unique_ptr<StoreIf>&   kv_store_;


    // 其下一条就是要执行的指令，初始化值为0
    uint64_t apply_instance_id_;

    // 是否正在执行快照操作
    SnapshotProgress snapshot_progress_;
    std::mutex apply_mutex_;
    std::condition_variable apply_notify_;

    // 保存状态机的执行结果
    std::mutex apply_rsp_mutex_;
    std::map<uint16_t, std::string> apply_rsp_;

    bool main_executor_stop_;
    std::thread main_executor_;
};


} // end namespace rong

#endif // __PAXOS_STATE_MACHINE__

