/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <system/ConstructException.h>

#include <other/Log.h>

#include <Paxos/PaxosConsensus.h>
#include <Paxos/LevelDBLog.h>
#include <Paxos/LevelDBStore.h>
#include <Paxos/StateMachine.h>

#include <message/ProtoBuf.h>
#include <Protocol/gen-cpp/Client.pb.h>

namespace rong {

StateMachine::StateMachine(PaxosConsensus& paxos_consensus,
                           std::unique_ptr<LogIf>& log_meta, std::unique_ptr<StoreIf>& kv_store) :
    paxos_consensus_(paxos_consensus),
    log_meta_(log_meta),
    kv_store_(kv_store),
    apply_instance_id_(0),
    snapshot_progress_(SnapshotProgress::kDone),
    apply_mutex_(),
    apply_notify_(),
    apply_rsp_mutex_(),
    apply_rsp_(),
    main_executor_stop_(false) {

    if (!log_meta_ || !kv_store)
        throw roo::ConstructException("Construct StateMachine failed, invalid log_meta and kv_store provided.");
}

bool StateMachine::init() {

    // 加载状态机执行位置
    apply_instance_id_ = log_meta_->meta_apply_instance_id();

    main_executor_ = std::thread(&StateMachine::state_machine_loop, this);

    roo::log_warning("detected applyed instance_id from %lu.", apply_instance_id_);
    roo::log_warning("Init StateMachine successfully.");
    return true;
}

void StateMachine::state_machine_loop() {

    while (!main_executor_stop_) {

        {
            auto expire_tp = std::chrono::system_clock::now() + std::chrono::seconds(5);

            std::unique_lock<std::mutex> lock(apply_mutex_);
#if __cplusplus >= 201103L
            apply_notify_.wait_until(lock, expire_tp);
#else
            apply_notify_.wait_until(lock, expire_tp);
#endif
        }

        LogIf::EntryPtr entry{};

     again:

        if (snapshot_progress_ == SnapshotProgress::kBegin)
            snapshot_progress_ = SnapshotProgress::kProcessing;

        if (snapshot_progress_ == SnapshotProgress::kProcessing) {
            roo::log_info("Snapshot is processing, skip this loop ...");
            continue;
        }

        // 伪唤醒，无任何处理
        // 但是防止客户端死锁，还是通知一下
        if (log_meta_->last_index() == apply_instance_id_) {
            paxos_consensus_.client_notify();
            continue;
        }

        // 无论成功失败，都前进
        entry = log_meta_->entry(apply_instance_id_ + 1);

        ApplyResponseType resp {};
        resp.set_instance_id(entry->instance_id());
        resp.set_node_id(entry->node_id());
        resp.set_context(entry->context());
        
        std::string content;
        int code = do_apply(entry, content);
        if (code == 0)
            resp.set_data(content);
        resp.set_code(code);

        {
            // local store 
            std::lock_guard<std::mutex> lock(apply_rsp_mutex_);
            apply_rsp_[apply_instance_id_ + 1] = resp;
        }

        roo::log_info("Applied entry at current processing apply_index %lu.",
                      apply_instance_id_ + 1);

        // step forward apply_index
        ++apply_instance_id_;
        log_meta_->set_meta_apply_instance_id(apply_instance_id_);

        // 如果提交队列有大量的日志需要执行，则不进行上述notify的等待
        if (log_meta_->last_index() > apply_instance_id_) {
            if (apply_instance_id_ % 20 == 0)
                paxos_consensus_.client_notify();

            goto again;
        }

        paxos_consensus_.client_notify();
    }

}


// do your specific business work here.
int StateMachine::do_apply(LogIf::EntryPtr entry, std::string& content_out) {

    std::string instruction = entry->data();

    rong::Client::StateMachineUpdateOps::Request request;
    if (!roo::ProtoBuf::unmarshalling_from_string(instruction, &request)) {
        roo::log_err("ProtoBuf unmarshal StateMachineWriteOps Request failed.");
        return -1;
    }

    int ret =  kv_store_->update_handle(request);

    if (ret == 0)
        content_out = "success at statemachine";
    else
        content_out = "failure at statemachine";

    return ret;
}


bool StateMachine::fetch_response_msg(uint64_t instance_id, ApplyResponseType& content) {

    std::lock_guard<std::mutex> lock(apply_rsp_mutex_);

    auto iter = apply_rsp_.find(instance_id);
    if (iter == apply_rsp_.end())
        return false;

    content = iter->second;

    // 删除掉
    apply_rsp_.erase(instance_id);

    // 剔除掉部分过于历史的响应

    //   uint64_t low_limit = apply_instance_id_ < 5001 ? 1 : apply_instance_id_ - 5000;
    //   auto low_bound = apply_rsp_.lower_bound(low_limit);
    //   apply_rsp_.erase(apply_rsp_.begin(), low_bound);

    return true;
}


bool StateMachine::create_snapshot(uint64_t& last_included_index, uint64_t& last_included_term) {
#if 0
    snapshot_progress_ = SnapshotProgress::kBegin;
    notify_state_machine();
    while (snapshot_progress_ != SnapshotProgress::kProcessing) {
        ::usleep(10);
    }

    roo::log_warning("Begin to create snapshot ...");
    if (apply_instance_id_ == 0) {
        roo::log_warning("StateMachine is initially state, give up.");
        snapshot_progress_ = SnapshotProgress::kDone;
        return true;
    }

    auto entry = log_meta_->entry(apply_instance_id_);
    if (!entry) {
        roo::log_err("Get entry at %lu failed.", apply_instance_id_);
        snapshot_progress_ = SnapshotProgress::kDone;
        return false;
    }

    bool result = kv_store_->create_snapshot(apply_instance_id_, entry->term());
    if (result) {
        last_included_index = apply_instance_id_;
        last_included_term  = entry->term();
    }

    snapshot_progress_ = SnapshotProgress::kDone;
    return result;
#endif

    return false;
}

bool StateMachine::load_snapshot(std::string& content, uint64_t& last_included_index, uint64_t& last_included_term) {

#if 0
    snapshot_progress_ = SnapshotProgress::kBegin;
    notify_state_machine();
    while (snapshot_progress_ != SnapshotProgress::kProcessing) {
        ::usleep(10);
    }

    roo::log_warning("Begin to load snapshot ...");
    bool result =  kv_store_->load_snapshot(content, last_included_index, last_included_term);

    snapshot_progress_ = SnapshotProgress::kDone;
    return result;
#endif

    return false;
}

bool StateMachine::apply_snapshot(const Snapshot::SnapshotContent& snapshot) {

    snapshot_progress_ = SnapshotProgress::kBegin;
    notify_state_machine();
    while (snapshot_progress_ != SnapshotProgress::kProcessing) {
        ::usleep(10);
    }

    roo::log_warning("Begin to apply snapshot ...");
    bool result = kv_store_->apply_snapshot(snapshot);

    snapshot_progress_ = SnapshotProgress::kDone;
    return result;
}

} // namespace rong
