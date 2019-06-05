/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <concurrency/Timer.h>
#include <scaffold/Setting.h>
#include <other/Log.h>
#include <string/StrUtil.h>
#include <other/FilesystemUtil.h>

#include <Protocol/Common.h>

#include <Paxos/Clock.h>
#include <Paxos/PaxosConsensus.h>

#include <Captain.h>

namespace rong {


bool PaxosConsensus::init() {

    auto setting_ptr = Captain::instance().setting_ptr_->get_setting();
    if (!setting_ptr) {
        roo::log_err("Request setting from Captain failed.");
        return false;
    }

    setting_ptr->lookupValue("Raft.bootstrap", option_.bootstrap_);
    setting_ptr->lookupValue("Raft.server_id", option_.id_);
    setting_ptr->lookupValue("Raft.storage_prefix", option_.log_path_);
    setting_ptr->lookupValue("Raft.log_trans_count", option_.log_trans_count_);

    uint64_t heartbeat_ms;
    uint64_t election_timeout_ms;
    uint64_t raft_distr_timeout_ms;
    setting_ptr->lookupValue("Raft.heartbeat_ms", heartbeat_ms);
    setting_ptr->lookupValue("Raft.election_timeout_ms", election_timeout_ms);
    setting_ptr->lookupValue("Raft.raft_distr_timeout_ms", raft_distr_timeout_ms);
    option_.heartbeat_ms_ = duration(heartbeat_ms);
    option_.election_timeout_ms_ = duration(election_timeout_ms);
    option_.raft_distr_timeout_ms_ = duration(raft_distr_timeout_ms);

    // 作为必填配置参数处理
    if(option_.raft_distr_timeout_ms_.count() == 0)
        option_.raft_distr_timeout_ms_ = option_.election_timeout_ms_;
    
    // if not found, will throw exceptions
    const libconfig::Setting& peers = setting_ptr->lookup("Raft.cluster_peers");
    for (int i = 0; i < peers.getLength(); ++i) {

        uint64_t id;
        std::string addr;
        uint64_t port;
        const libconfig::Setting& peer = peers[i];

        peer.lookupValue("server_id", id);
        peer.lookupValue("addr", addr);
        peer.lookupValue("port", port);

        if (id == 0 || addr.empty() || port == 0) {
            roo::log_err("Find problem peer setting: id %lu, addr %s, port %lu, skip this member.", id, addr.c_str(), port);
            continue;
        }

        if (option_.members_.find(id) != option_.members_.end()) {
            roo::log_err("This node already added before: id %lu, addr %s, port %lu.", id, addr.c_str(), port);
            continue;
        }

        option_.members_[id] = std::make_pair(addr, port);
        option_.members_str_ += roo::StrUtil::to_string(id) + ">" + addr + ":" + roo::StrUtil::to_string(port) + ",";
    }

    option_.withhold_votes_ms_ = option_.election_timeout_ms_;
    if (!option_.validate()) {
        roo::log_err("Validate raft option failed, please check the configuration file!");
        return false;
    }
    roo::log_info("Current setting dump for node %lu:\n %s", option_.id_, option_.str().c_str());

    // 随机化选取超时定时器
    ::srand(::time(NULL) + option_.id_);
    if (option_.election_timeout_ms_.count() > 3)
        option_.election_timeout_ms_ += duration(::random() % (option_.election_timeout_ms_.count() / 3));

    // 初始化 peer_map_ 的主机列表
    for (auto iter = option_.members_.begin(); iter != option_.members_.end(); ++iter) {
        auto endpoint = iter->second;
        auto peer = std::make_shared<Peer>(iter->first, endpoint.first, endpoint.second,
                                           std::bind(&PaxosConsensus::handle_rpc_callback, this,
                                                     std::placeholders::_1, std::placeholders::_2,
                                                     std::placeholders::_3, std::placeholders::_4));
        if (!peer) {
            roo::log_err("Create peer member instance %lu failed.", iter->first);
            return false;
        }

        peer_set_[iter->first] = peer;
    }
    roo::log_warning("Totally detected and successfully initialized %lu peers!", peer_set_.size());

    return true;
}


std::shared_ptr<Peer> PaxosConsensus::get_peer(uint64_t peer_id) const {
    auto peer = peer_set_.find(peer_id);
    if (peer == peer_set_.end()) {
        roo::log_err("Bad, request peer_id's id(%lu) is not in peer_set!", peer_id);
        return{ };
    }

    return peer->second;
}


int PaxosConsensus::handle_rpc_callback(RpcClientStatus status, uint16_t service_id, uint16_t opcode, const std::string& rsp) {

    if (status != RpcClientStatus::OK) {
        roo::log_err("RPC call failed with status code %d, for service_id %u and opcode %u.",
                     static_cast<uint8_t>(status), service_id, opcode);
        return -1;
    }

    if (service_id != static_cast<uint16_t>(tzrpc::ServiceID::PAXOS_SERVICE)) {
        roo::log_err("Recived callback with invalid service_id %u, expect %d",
                     service_id, tzrpc::ServiceID::PAXOS_SERVICE);
        return -1;
    }
#if 0
    if (opcode == static_cast<uint16_t>(OpCode::kRequestVote) ||
        opcode == static_cast<uint16_t>(OpCode::kAppendEntries) ||
        opcode == static_cast<uint16_t>(OpCode::kInstallSnapshot)) {
        // 添加到延迟队列
        auto func = std::bind(&RaftConsensus::continue_bf_async, this, opcode, rsp);
        defer_cb_task_.add_defer_task(func);
        return 0;
    }
#endif
    roo::log_err("Unexpected RPC call response with opcode %u", opcode);
    return -1;
}



} // namespace rong
