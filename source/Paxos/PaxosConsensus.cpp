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

#include <Paxos/LevelDBLog.h>
#include <Paxos/LevelDBStore.h>
#include <Paxos/Clock.h>
#include <Paxos/PaxosConsensus.h>

#include <Paxos/BasicProposer.h>
#include <Paxos/BasicAcceptor.h>
#include <Paxos/BasicLearner.h>

#include <Captain.h>

namespace rong {


bool PaxosConsensus::init() {

    auto setting_ptr = Captain::instance().setting_ptr_->get_setting();
    if (!setting_ptr) {
        roo::log_err("Request setting from Captain failed.");
        return false;
    }

    setting_ptr->lookupValue("Paxos.bootstrap", option_.bootstrap_);
    setting_ptr->lookupValue("Paxos.server_id", option_.id_);
    setting_ptr->lookupValue("Paxos.storage_prefix", option_.log_path_);

    uint64_t master_lease_election_ms;
    uint64_t master_lease_heartbeat_ms;
    uint64_t prepare_propose_timeout_ms;
    setting_ptr->lookupValue("Paxos.master_lease_election_ms", master_lease_election_ms);
    setting_ptr->lookupValue("Paxos.master_lease_heartbeat_ms", master_lease_heartbeat_ms);
    setting_ptr->lookupValue("Paxos.prepare_propose_timeout_ms", prepare_propose_timeout_ms);
    option_.master_lease_election_ms_ = duration(master_lease_election_ms);
    option_.master_lease_heartbeat_ms_ = duration(master_lease_heartbeat_ms);
    option_.prepare_propose_timeout_ms_ = duration(prepare_propose_timeout_ms);

    // if not found, will throw exceptions
    const libconfig::Setting& peers = setting_ptr->lookup("Paxos.cluster_peers");
    for (int i = 0; i < peers.getLength(); ++i) {

        uint64_t id;
        std::string addr;
        uint64_t port;
        const libconfig::Setting& peer = peers[i];

        peer.lookupValue("server_id", id);
        peer.lookupValue("addr", addr);
        peer.lookupValue("port", port);

        if (id == 0 || addr.empty() || port == 0) {
            roo::log_err("Find problem peer setting: id %lu, addr %s, port %lu, skip this member.",
                         id, addr.c_str(), port);
            continue;
        }

        if (option_.members_.find(id) != option_.members_.end()) {
            roo::log_err("This node already added before: id %lu, addr %s, port %lu.",
                         id, addr.c_str(), port);
            continue;
        }

        option_.members_[id] = std::make_pair(addr, port);
        option_.members_str_ += roo::StrUtil::to_string(id) + ">" + addr + ":" + roo::StrUtil::to_string(port) + ",";
    }

    if (!option_.validate()) {
        roo::log_err("Validate raft option failed, please check the configuration file!");
        return false;
    }
    roo::log_info("Current setting dump for node %lu:\n %s", option_.id_, option_.str().c_str());

    // 随机化选取超时定时器
    ::srand(::time(NULL) + option_.id_);
    if (option_.master_lease_election_ms_.count() > 3)
        option_.master_lease_election_ms_ +=
            duration(::random() % (option_.master_lease_election_ms_.count() / 10));

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

        cluster_set_[iter->first] = peer;
    }
    roo::log_warning("Totally detected and successfully initialized %lu peers!", cluster_set_.size());

    // adjust log store path
    option_.log_path_ += "/instance_" + roo::StrUtil::to_string(option_.id_);
    if (!roo::FilesystemUtil::exists(option_.log_path_)) {
        ::mkdir(option_.log_path_.c_str(), 0755);
        if (!roo::FilesystemUtil::exists(option_.log_path_)) {
            roo::log_err("Create node base storage directory failed: %s.", option_.log_path_.c_str());
            return false;
        }
    }

    log_meta_ = make_unique<LevelDBLog>(option_.log_path_ + "/log_meta");
    if (!log_meta_) {
        roo::log_err("Create LevelDBLog handle failed.");
        return false;
    }

    kv_store_ = make_unique<LevelDBStore>(option_.log_path_ + "/kv_store", option_.log_path_ + "/snapshot/");
    if (!kv_store_) {
        roo::log_err("Create LevelDBStore handle failed.");
        return false;
    }

    // 加载持久化的元信息，主要再崩溃恢复时候使用
    PaxosMetadata::Metadata meta;
    if (log_meta_->meta_data(&meta) != 0) {
        roo::log_err("Load PaxosMetadata from storage failed.");
        return false;
    }

    // 状态机执行线程
    state_machine_ = make_unique<StateMachine>(*this, log_meta_, kv_store_);
    if (!state_machine_ || !state_machine_->init()) {
        roo::log_err("Create and initialize StateMachine failed.");
        return false;
    }


    context_ = std::make_shared<Context>(option_.id_, log_meta_);
    if (!context_ || !context_->init(meta)) {
        roo::log_err("PaxosConsensus init Context failed.");
        return false;
    }


#if 0
    // 初始化MasterLease选主模块
    if (!master_lease_.init()) {
        roo::log_err("MasterLease init failed.");
        return false;
    }

    // start to acquire
    master_lease_.acquire_lease();
#endif

    proposer_.reset(new BasicProposer(*this));
    acceptor_.reset(new BasicAcceptor(*this, meta));
    learner_.reset(new BasicLearner(*this, log_meta_));

    // 主工作线程
    main_thread_ = std::thread(std::bind(&PaxosConsensus::main_thread_loop, this));

    return true;
}


std::shared_ptr<Peer> PaxosConsensus::get_peer(uint64_t peer_id) const {
    auto peer = cluster_set_.find(peer_id);
    if (peer == cluster_set_.end()) {
        roo::log_err("Bad, request peer_id's id(%lu) is not in peer_set!", peer_id);
        return{ };
    }

    return peer->second;
}

int PaxosConsensus::handle_paxos_lease_request(const Paxos::LeaseMessage& request, Paxos::LeaseMessage& response) {
//    return master_lease_.handle_paxos_lease(request, response);
    return -1;
}

int PaxosConsensus::handle_paxos_basic_request(const Paxos::BasicMessage& request, Paxos::BasicMessage& response) {

    // 更新全局的最新instance_id
    if (request.has_instance_id() && context_->highest_instance_id() < request.instance_id()) {
        context_->update_highest_instance_id(request.instance_id());
    }

    if (request.type() == Paxos::kBPrepareRequest) {
        acceptor_->on_prepare_request(request, response);
        return 0;
    } else if (request.type() == Paxos::kBProposeRequest) {
        acceptor_->on_propose_request(request, response);
        return 0;
    } else if (request.type() == Paxos::kBProposeLearnValue ||
               request.type() == Paxos::kBProposeChosenValue ) {
        
        // skip local message
        if (request.node_id() == context_->kID)
            return 0;

        learner_->on_learn_request(request, response);

        if (request.type() == Paxos::kBProposeLearnValue) {

            // 如果本机的日志不全
            if (response.instance_id() < request.instance_id()) {
                Paxos::BasicMessage message {};
                message.set_type(Paxos::kBProposeChosenValue);
                message.set_node_id(context_->kID);
                message.set_proposal_id(current_instance_id());
                message.set_instance_id(response.instance_id() + 1); // 表示需要的日志条目索引号

                send_paxos_basic(request.node_id(), message);
                roo::log_warning("Expecting log from %lu.", response.instance_id());
            }
        }
        return 0;
    }

    roo::log_err("Unhandle paxos_basic request type found: %d", static_cast<int>(request.type()));
    return -1;
}

int PaxosConsensus::handle_paxos_lease_response(Paxos::LeaseMessage response) {

    roo::log_err("NOT IMPLEMENTED...");
    return -1;
}


int PaxosConsensus::handle_paxos_basic_response(Paxos::BasicMessage response) {

    if (response.type() == Paxos::kBPrepareRejected ||
        response.type() == Paxos::kBPreparePreviouslyAccepted ||
        response.type() == Paxos::kBPrepareCurrentlyOpen) {
        proposer_->on_prepare_response(response);
        return 0;
    } else if (response.type() == Paxos::kBProposeRejected ||
               response.type() == Paxos::kBProposeAccepted) {
        proposer_->on_propose_response(response);
        return 0;
    } else if (response.type() == Paxos::kBProposeLearnResponse ||
               response.type() == Paxos::kBProposeChosenResponse ) {
        learner_->on_learn_response(response);

        if (response.type() == Paxos::kBProposeChosenResponse) {

            // 如果本机的日志不全
            if (log_meta_->last_index() < response.log_last_index()) {
                Paxos::BasicMessage message {};
                message.set_type(Paxos::kBProposeChosenValue);
                message.set_node_id(context_->kID);
                message.set_proposal_id(current_instance_id());
                message.set_instance_id(log_meta_->last_index() + 1); // 表示需要的日志条目索引号

                send_paxos_basic(response.node_id(), message);
                roo::log_warning("Expecting log from %lu at %lu.", response.node_id(), message.instance_id());
            }
        }

        return 0;
    }

    roo::log_err("Unhandle paxos_basic response type found: %d", static_cast<int>(response.type()));
    return -1;
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

    if (opcode == static_cast<uint16_t>(Paxos::OpCode::kPaxosLease)) {
        rong::Paxos::LeaseMessage response;
        if (!roo::ProtoBuf::unmarshalling_from_string(rsp, &response)) {
            roo::log_err("unmarshal response failed.");
            return -1;
        }

        auto func = std::bind(&PaxosConsensus::handle_paxos_lease_response, this, response);
        defer_cb_task_.add_defer_task(func);
        return 0;
    } else if (opcode == static_cast<uint16_t>(Paxos::OpCode::kPaxosBasic)) {
        rong::Paxos::BasicMessage response;
        if (!roo::ProtoBuf::unmarshalling_from_string(rsp, &response)) {
            roo::log_err("unmarshal response failed.");
            return -1;
        }

        auto func = std::bind(&PaxosConsensus::handle_paxos_basic_response, this, response);
        defer_cb_task_.add_defer_task(func);
        return 0;
    }

    roo::log_err("Unexpected RPC call response with opcode %u", opcode);
    return -1;
}

uint64_t PaxosConsensus::current_leader() const {
    return 0;
}

bool PaxosConsensus::is_leader() const {
    return false;
}

// 本函数的调用是在statemachine中被调用的，会被client_mutex_保护，所以单个节点中
// 肯定是串行没有并发问题的
// 但是BasicPaxos允许任意的节点发起提案，则可能会有instanceID的冲突
// 对此，Paxos肯定能够保证一个instanceID中只有一个value被chosen
// TODO:
// 所以需要能够检测到这种情况，然后当客户端发现本instanceID被占用后，进行放弃或者重新尝试
bool PaxosConsensus::startup_instance() {

    // 不允许日志过于落后的instance发起请求，否则后续的日志填充将会比较麻烦
    // 不过我们允许至少一个on-fly的日志，否则异常情况无法启动
    if (context_->highest_instance_id() > log_meta_->last_index() + 1)
        return -1;

    uint64_t current_instance_id = 0;
    if (!context_->startup_instance(current_instance_id))
        return false;

    acceptor_->state().startup();
    proposer_->state().startup();
    learner_->state().startup();

    proposer_->state().instanceID = current_instance_id;

    roo::log_info("startup new instance_id %lu.", current_instance_id);
    return true;
}

void PaxosConsensus::close_instance() {

    context_->close_instance();

    acceptor_->state().startup();
    proposer_->state().startup();
    learner_->state().startup();

    roo::log_info("close instance_id %lu.", proposer_->state().instanceID);
}

uint64_t PaxosConsensus::current_instance_id() const {
    return proposer_->state().instanceID;
}

uint64_t PaxosConsensus::highest_instance_id() const {
    return context_->highest_instance_id();
}


int PaxosConsensus::state_machine_update(const std::string& cmd, std::string& apply_out) {

    std::unique_lock<std::mutex> lock(client_mutex_);

    while (!startup_instance())
        client_notify_.wait(lock);

    uint64_t instance_id = proposer_->state().instanceID;

    // 发起propose
    proposer_->propose(cmd);

    // 等待发起的值被Chosen以及状态机执行
    while (state_machine_->apply_instance_id() < instance_id) {
        client_notify_.wait(lock);
    }

    // 获取状态机执行的缓存结果
    Client::StateMachineUpdateOps::Response response;
    response.set_code(0);
    response.set_msg("OK");

    std::string content;
    if (state_machine_->fetch_response_msg(instance_id, content))
        response.set_context(content);

    roo::ProtoBuf::marshalling_to_string(response, &apply_out);

    context_->close_instance();
    return 0;
}

// 暂时的 stale 读
int PaxosConsensus::state_machine_select(const std::string& cmd, std::string& query_out) {

    Client::StateMachineSelectOps::Request  request;
    if (!roo::ProtoBuf::unmarshalling_from_string(cmd, &request)) {
        roo::log_err("unmarshal request failed.");
        return -1;
    }

    Client::StateMachineSelectOps::Response response;
    int ret = kv_store_->select_handle(request, response);
    if (ret != 0) {
        roo::log_err("kv store do query failed with %d.", ret);
        return ret;
    }

    response.set_code(0);
    response.set_msg("OK");
    roo::ProtoBuf::marshalling_to_string(response, &query_out);

    return 0;
}

// 此处只会写入日志，当状态机发现日志不完整的时候，会自动向其他Peer发起学习请求
int PaxosConsensus::append_chosen(uint64_t index, const std::string& val) {

    if (index > log_meta_->last_index() + 1) {
        PANIC("LearnLog GAP found, desired %lu, but last_index %lu.",
              index, log_meta_->last_index() + 1);
    }

    auto entry = std::make_shared<LogIf::Entry>();
    if (!entry) {
        roo::log_err("Create new entry failed.");
        return -1;
    }

    entry->set_type(Paxos::EntryType::kNormal);
    entry->set_data(val);

    auto code = log_meta_->append(index, entry);
    state_machine_->notify_state_machine();

    return code;
}

void PaxosConsensus::main_thread_loop() {

    while (!main_thread_stop_) {

        {
            std::unique_lock<std::mutex> lock(consensus_mutex_);

            auto expire_tp = steady_clock::now() + std::chrono::milliseconds(100);
#if __cplusplus >= 201103L
            consensus_notify_.wait_until(lock, expire_tp);
#else
            consensus_notify_.wait_until(lock, expire_tp);
#endif
        }

        if (prepare_propose_timer_.timeout(option_.prepare_propose_timeout_ms_)) {
            if (proposer_->state().preparing) {
                proposer_->start_preparing();
            } else if (proposer_->state().proposing) {
                // 如果在Accept阶段超时了，则从新从第一阶段Prepare开始重新发起提议
                proposer_->start_preparing();
            } else {
                PANIC("Invalid proposer state with timeout specified.");
            }
        }
    }
}

} // namespace rong
