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
#include <Protocol/gen-cpp/Paxos.pb.h>

#include <Paxos/Peer.h>
#include <Paxos/Option.h>
#include <Paxos/Clock.h>

#include <Client/include/RpcClient.h>
#include <Client/include/RpcClientStatus.h>

namespace tzrpc {
class PaxosService;
class ClientService;
}

namespace rong {

using tzrpc_client::RpcClientStatus;
class Clock;

class PaxosConsensus {

    // access internal do_handle_xxx_request
    friend class tzrpc::PaxosService;

    // access internal kv_store_
    friend class tzrpc::ClientService;

    // access internal consensus_notify_
    friend class Clock;

    __noncopyable__(PaxosConsensus)

public:

    typedef rong::Paxos::OpCode  OpCode;

    PaxosConsensus():
        option_(),
        peer_set_(){
    }

    ~PaxosConsensus() {
    }

    bool init();

    // 对于向peer发送的rpc请求，其响应都会在这个函数中异步执行
    int handle_rpc_callback(RpcClientStatus status, uint16_t service_id, uint16_t opcode, const std::string& rsp);

    std::shared_ptr<Peer> get_peer(uint64_t peer_id) const;

private:

    Option option_;

    // 实例的全局互斥保护
    // 因为涉及到的模块比较多，所以该锁是一个模块全局性的大锁
    // 另外，锁的特性是不可重入的，所以要避免死锁
    std::mutex consensus_mutex_;
    std::condition_variable consensus_notify_;

    // Timer
    SimpleTimer heartbeat_timer_;
    SimpleTimer election_timer_;
    SimpleTimer withhold_votes_timer_;

    // current static conf, not protected
    std::map<uint64_t, std::shared_ptr<Peer>> peer_set_;

};

} // namespace rong

#endif // __PAXOS_PAXOS_CONSENSUS_H__
