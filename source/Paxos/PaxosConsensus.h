/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __PAXOS_PAXOS_CONSENSUS_H__
#define __PAXOS_PAXOS_CONSENSUS_H__

#include <xtra_rhel.h>
#include <thread>


#include <message/ProtoBuf.h>
#include <container/EQueue.h>
#include <concurrency/DeferTask.h>

#include <Protocol/gen-cpp/Paxos.pb.h>
#include <Protocol/Common.h>

#include <Paxos/Peer.h>
#include <Paxos/Option.h>
#include <Paxos/Context.h>
#include <Paxos/Clock.h>
#include <Paxos/MasterLease.h>
#include <Paxos/LogIf.h>
#include <Paxos/StoreIf.h>
#include <Paxos/StateMachine.h>

#include <Client/include/RpcClient.h>
#include <Client/include/RpcClientStatus.h>

namespace tzrpc {
class PaxosService;
class ClientService;
}

namespace rong {

using tzrpc_client::RpcClientStatus;
class Clock;

class BasicProposer;
class BasicAcceptor;
class BasicLearner;

class PaxosConsensus {

    // access internal do_handle_xxx_request
    friend class tzrpc::PaxosService;

    // access internal kv_store_
    friend class tzrpc::ClientService;

    // access internal consensus_notify_
    friend class Clock;

    friend class MasterLease;

    __noncopyable__(PaxosConsensus)

public:

    typedef rong::Paxos::OpCode  OpCode;

    PaxosConsensus() :
        option_(),
        context_(),
//        master_lease_(*this),
        peer_set_() {
    }

    ~PaxosConsensus() = default;

    bool init();

    // 调用该接口发送消息给Peer
    void send_paxos_lease(const Paxos::LeaseMessage& request) {
        std::string msg;
        roo::ProtoBuf::marshalling_to_string(request, &msg);

        for (auto iter = peer_set_.begin(); iter != peer_set_.end(); ++iter) {
            const auto peer = iter->second;
            roo::log_info("Send kPaxosBasic to %s:%d.", peer->addr_.c_str(), peer->port_);
            peer->send_paxos_RPC(tzrpc::ServiceID::PAXOS_SERVICE, Paxos::OpCode::kPaxosLease, msg);
        }
    }

    void send_paxos_basic(const Paxos::BasicMessage& request) {
        std::string msg;
        roo::ProtoBuf::marshalling_to_string(request, &msg);

        for (auto iter = peer_set_.begin(); iter != peer_set_.end(); ++iter) {
            const auto peer = iter->second;
            roo::log_info("Send kPaxosBasic to %s:%d.", peer->addr_.c_str(), peer->port_);
            peer->send_paxos_RPC(tzrpc::ServiceID::PAXOS_SERVICE, Paxos::OpCode::kPaxosBasic, msg);
        }
    }

    // Paxos协议相关的RPC直接响应请求
    int handle_paxos_lease_request(const Paxos::LeaseMessage& request, Paxos::LeaseMessage& response);
    int handle_paxos_basic_request(const Paxos::BasicMessage& request, Paxos::BasicMessage& response);

    // 对于向peer发送的rpc请求，其响应都会在这个函数中异步执行
    int handle_rpc_callback(RpcClientStatus status, uint16_t service_id, uint16_t opcode,
                            const std::string& rsp);
    int handle_paxos_lease_response(Paxos::LeaseMessage response);
    int handle_paxos_basic_response(Paxos::BasicMessage response);

    std::shared_ptr<Peer> get_peer(uint64_t peer_id) const;

    uint64_t current_leader() const;
    bool is_leader() const;
    size_t quorum_count() const {
        //return static_cast<size_t>(::floor((double)(peer_set_.size() + 1) / 2) + 1);
        return static_cast<size_t>(((peer_set_.size() + 1) >> 1) + 1);
    }

    uint64_t next_proposal_id(uint64_t hint) const {
        return context_->next_proposal_id(hint);
    }

    bool startup_instance();
    void close_instance();
    uint64_t current_instance_id() const;
    uint64_t highest_instance_id() const;

    // 客户端的请求
    int state_machine_update(const std::string& cmd, std::string& apply_out);
    int state_machine_select(const std::string& cmd, std::string& query_out);

    // 状态机的快照处理
    int append_chosen(uint64_t index, const std::string& val);
    int state_machine_snapshot();

    void consensus_notify() { consensus_notify_.notify_all(); }
    void client_notify() { client_notify_.notify_all(); }


public:

    Option option_;

    // 一些易变、需要持久化的数据集
    std::shared_ptr<Context> context_;

    // 实例的全局互斥保护
    // 因为涉及到的模块比较多，所以该锁是一个模块全局性的大锁
    // 另外，锁的特性是不可重入的，所以要避免死锁
    std::mutex consensus_mutex_;
    std::condition_variable consensus_notify_;

    // 用于除上面互斥和信号量保护之外的用途，主要是客户端请求需要
    // 改变状态机的时候
    std::mutex client_mutex_;
    std::condition_variable client_notify_;

    // PaxosLease选主
    // MasterLease master_lease_;

    // current static conf, not protected
    std::map<uint64_t, std::shared_ptr<Peer>> peer_set_;

    // Paxos log & meta store
    std::unique_ptr<LogIf> log_meta_;

    // 状态机处理模块，机器对应的LevelDB底层存储模块
    std::unique_ptr<StateMachine> state_machine_;
    std::unique_ptr<StoreIf> kv_store_;

    // 简化起见，本实现对于Paxos的Instance是严格的依次序列化执行的，即只有本处理的
    // Instance进行Accept、Learn之后才会正常关闭，然后再递增paxosID进行下一个Instance的处理
    // 虽然通过窗口的形式并行执行多个Instance可能会增加性能，但是对于持久状态的保存和崩溃后的处理
    // 是十分复杂的
    std::shared_ptr<BasicProposer> proposer_;
    std::shared_ptr<BasicAcceptor> acceptor_;
    std::shared_ptr<BasicLearner>  learner_;

    // Timer
    SimpleTimer prepare_propose_timer_;

    bool main_thread_stop_;
    std::thread main_thread_;
    void main_thread_loop();

    // 对于回调任务，都丢到一个队列中，然后使用这个异步线程执行(因为基本都是加锁了的，所以这边就直接用一个线程)
    roo::DeferTask defer_cb_task_;

};

} // namespace rong

#endif // __PAXOS_PAXOS_CONSENSUS_H__
