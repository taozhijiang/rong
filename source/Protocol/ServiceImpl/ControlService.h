#ifndef __PROTOCOL_CONTROL_SERVICE_H__
#define __PROTOCOL_CONTROL_SERVICE_H__

#include <xtra_rhel.h>

#include <RPC/Service.h>
#include <RPC/RpcInstance.h>

#include <scaffold/Setting.h>

#include "RpcServiceBase.h"

// 用于处理客户端的请求

namespace tzrpc {

class ControlService : public Service,
    public RpcServiceBase {

public:
    explicit ControlService(const std::string& instance_name) :
        RpcServiceBase(instance_name),
        instance_name_(instance_name) {
    }

    ~ControlService() = default;

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
    void control_snapshot_impl(std::shared_ptr<RpcInstance> rpc_instance);
    void control_stat_impl(std::shared_ptr<RpcInstance> rpc_instance);

    const std::string instance_name_;

};

} // namespace tzrpc

#endif // __PROTOCOL_CONTROL_SERVICE_H__
