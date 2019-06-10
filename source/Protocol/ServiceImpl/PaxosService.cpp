#include <other/Log.h>

#include <message/ProtoBuf.h>
#include <Protocol/gen-cpp/Paxos.pb.h>

#include <scaffold/Setting.h>
#include <scaffold/Status.h>

#include <Captain.h>
#include <Paxos/PaxosConsensus.h>

#include "PaxosService.h"

namespace tzrpc {

using rong::Captain;


bool PaxosService::init() {

    auto setting_ptr = Captain::instance().setting_ptr_->get_setting();
    if (!setting_ptr) {
        roo::log_err("Setting not initialized? return setting_ptr empty!!!");
        return false;
    }

    bool init_success = false;

    try {

        const libconfig::Setting& rpc_services = setting_ptr->lookup("rpc.services");

        for (int i = 0; i < rpc_services.getLength(); ++i) {

            const libconfig::Setting& service = rpc_services[i];

            // 跳过没有配置instance_name的配置
            std::string instance_name;
            service.lookupValue("instance_name", instance_name);
            if (instance_name.empty()) {
                roo::log_err("check service conf, required instance_name not found, skip this one.");
                continue;
            }

            roo::log_debug("detected instance_name: %s", instance_name.c_str());

            // 发现是匹配的，则找到对应虚拟主机的配置文件了
            if (instance_name == instance_name_) {
                if (!handle_rpc_service_conf(service)) {
                    roo::log_err("handle detail conf for instnace %s failed.", instance_name.c_str());
                    return false;
                }

                // OK, we will return
                roo::log_debug("handle detail conf for instance %s success!", instance_name.c_str());
                init_success = true;
                break;
            }
        }

    } catch (const libconfig::SettingNotFoundException& nfex) {
        roo::log_err("rpc.services not found!");
    } catch (std::exception& e) {
        roo::log_err("execptions catched for %s",  e.what());
    }

    if (!init_success) {
        roo::log_err("instance %s init failed, may not configure for it?", instance_name_.c_str());
    }

    return init_success;
}

// 系统启动时候初始化，持有整个锁进行
bool PaxosService::handle_rpc_service_conf(const libconfig::Setting& setting) {

    std::unique_lock<std::mutex> lock(conf_lock_);

    if (!conf_ptr_) {
        conf_ptr_.reset(new ServiceConf());
        if (!conf_ptr_) {
            roo::log_err("create ServiceConf instance failed.");
            return false;
        }
    }

    ExecutorConf conf;
    if (RpcServiceBase::handle_rpc_service_conf(setting, conf) != 0) {
        roo::log_err("Handler ExecutorConf failed.");
        return -1;
    }

    // 保存更新
    conf_ptr_->executor_conf_ = conf;

    // other conf handle may add code here...

    return true;
}



ExecutorConf PaxosService::get_executor_conf() {
    SAFE_ASSERT(conf_ptr_);
    return conf_ptr_->executor_conf_;
}

int PaxosService::module_runtime(const libconfig::Config& conf) {

    try {

        const libconfig::Setting& rpc_services = conf.lookup("rpc_services");

        for (int i = 0; i < rpc_services.getLength(); ++i) {

            const libconfig::Setting& service = rpc_services[i];
            std::string instance_name;
            service.lookupValue("instance_name", instance_name);

            // 发现是匹配的，则找到对应虚拟主机的配置文件了
            if (instance_name == instance_name_) {
                roo::log_notice("about to handle_rpc_service_runtime_conf update for %s", instance_name_.c_str());
                return handle_rpc_service_runtime_conf(service);
            }
        }

    } catch (const libconfig::SettingNotFoundException& nfex) {
        roo::log_err("rpc_services not found!");
    } catch (std::exception& e) {
        roo::log_err("execptions catched for %s",  e.what());
    }

    roo::log_err("conf for instance %s not found!!!!", instance_name_.c_str());
    return -1;
}

// 做一些可选的配置动态更新
bool PaxosService::handle_rpc_service_runtime_conf(const libconfig::Setting& setting) {

    ExecutorConf conf;
    if (RpcServiceBase::handle_rpc_service_conf(setting, conf) != 0) {
        roo::log_err("Handler ExecutorConf failed.");
        return -1;
    }

    {
        // do swap here
        std::unique_lock<std::mutex> lock(conf_lock_);
        conf_ptr_->executor_conf_ = conf;
    }

    return 0;
}

int PaxosService::module_status(std::string& module, std::string& name, std::string& val) {

    // empty status ...

    return 0;
}


void PaxosService::handle_RPC(std::shared_ptr<RpcInstance> rpc_instance) {

    using rong::Paxos::OpCode;

    // Call the appropriate RPC handler based on the request's opCode.
    switch (rpc_instance->get_opcode()) {
        case OpCode::kPaxosLease:
            paxos_lease_impl(rpc_instance);
            break;
        case OpCode::kPaxosBasic:
            paxos_basic_impl(rpc_instance);
            break;

        default:
            roo::log_err("Received RPC request with unknown opcode %u: "
                         "rejecting it as invalid request",
                         rpc_instance->get_opcode());
            rpc_instance->reject(RpcResponseStatus::INVALID_REQUEST);
    }
}


void PaxosService::paxos_lease_impl(std::shared_ptr<RpcInstance> rpc_instance) {

    RpcRequestMessage& rpc_request_message = rpc_instance->get_rpc_request_message();
    if (rpc_request_message.header_.opcode != rong::Paxos::OpCode::kPaxosLease) {
        roo::log_err("invalid opcode %u in service Paxos.", rpc_request_message.header_.opcode);
        rpc_instance->reject(RpcResponseStatus::INVALID_REQUEST);
        return;
    }

    rong::Paxos::LeaseMessage request;
    if (!roo::ProtoBuf::unmarshalling_from_string(rpc_request_message.payload_, &request)) {
        roo::log_err("unmarshal request failed.");
        rpc_instance->reject(RpcResponseStatus::INVALID_REQUEST);
        return;
    }

    rong::Paxos::LeaseMessage response;
    int ret = Captain::instance().paxos_consensus_ptr_->handle_paxos_lease_request(request, response);
    if (ret != 0) {
        roo::log_err("handle request_vote return %d", ret);
        rpc_instance->reject(RpcResponseStatus::SYSTEM_ERROR);
        return;
    }

    std::string response_str;
    roo::ProtoBuf::marshalling_to_string(response, &response_str);
    rpc_instance->reply_rpc_message(response_str);

    return;
}

void PaxosService::paxos_basic_impl(std::shared_ptr<RpcInstance> rpc_instance) {

    RpcRequestMessage& rpc_request_message = rpc_instance->get_rpc_request_message();
    if (rpc_request_message.header_.opcode != rong::Paxos::OpCode::kPaxosBasic) {
        roo::log_err("invalid opcode %u in service Paxos.", rpc_request_message.header_.opcode);
        rpc_instance->reject(RpcResponseStatus::INVALID_REQUEST);
        return;
    }

    rong::Paxos::BasicMessage request;
    if (!roo::ProtoBuf::unmarshalling_from_string(rpc_request_message.payload_, &request)) {
        roo::log_err("unmarshal request failed.");
        rpc_instance->reject(RpcResponseStatus::INVALID_REQUEST);
        return;
    }

    rong::Paxos::BasicMessage response;
    int ret = Captain::instance().paxos_consensus_ptr_->handle_paxos_basic_request(request, response);
    if (ret != 0) {
        roo::log_err("handle append_entries return %d", ret);
        rpc_instance->reject(RpcResponseStatus::SYSTEM_ERROR);
        return;
    }

    std::string response_str;
    roo::ProtoBuf::marshalling_to_string(response, &response_str);
    rpc_instance->reply_rpc_message(response_str);

    return;
}



} // namespace tzrpc
