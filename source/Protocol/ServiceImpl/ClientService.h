#ifndef __PROTOCOL_CLIENT_SERVICE_H__
#define __PROTOCOL_CLIENT_SERVICE_H__

#include <xtra_rhel.h>

#include <RPC/Service.h>
#include <RPC/RpcInstance.h>

#include <scaffold/Setting.h>

#include "RpcServiceBase.h"

// 用于处理客户端的请求

namespace tzrpc {

class ClientService : public Service,
    public RpcServiceBase {

public:
    explicit ClientService(const std::string& instance_name) :
        RpcServiceBase(instance_name),
        instance_name_(instance_name),
        paxos_instance_mutex_() {
    }

    ~ClientService() = default;

    void handle_RPC(std::shared_ptr<RpcInstance> rpc_instance);
    std::string instance_name() {
        return instance_name_;
    }

    bool init();

private:

    // 业务内部的配置信息登记
    struct ServiceConf {

        // 用来返回给Executor使用的，主要是线程伸缩相关这类的通用配置东西
        ExecutorConf executor_conf_;

        // other stuffs if needed, please add here

    };

    std::mutex conf_lock_;
    std::shared_ptr<ServiceConf> conf_ptr_;

    bool handle_rpc_service_conf(const libconfig::Setting& setting);
    bool handle_rpc_service_runtime_conf(const libconfig::Setting& setting);

    ExecutorConf get_executor_conf();
    int module_runtime(const libconfig::Config& conf);
    int module_status(std::string& module, std::string& name, std::string& val);


private:

    ////////// RPC handlers //////////

    void client_select_impl(std::shared_ptr<RpcInstance> rpc_instance);
    void client_update_impl(std::shared_ptr<RpcInstance> rpc_instance);

    const std::string instance_name_;

    // 下面的函数使用互斥锁来严格序列化依次执行，因为当前的
    // Paxos的开放Instance只能为1
    std::mutex paxos_instance_mutex_;
};

} // namespace tzrpc

#endif // __PROTOCOL_CLIENT_SERVICE_H__
