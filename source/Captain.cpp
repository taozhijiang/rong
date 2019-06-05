/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <cstdlib>

#include <memory>
#include <string>
#include <map>

#include <other/Log.h>
#include <Network/NetServer.h>

#include <concurrency/Timer.h>
#include <concurrency/IoService.h>

#include <scaffold/Setting.h>
#include <scaffold/Status.h>

#include <RPC/Dispatcher.h>
#include <Protocol/Common.h>
#include <Protocol/ServiceImpl/PaxosService.h>
#include <Protocol/ServiceImpl/ClientService.h>
#include <Protocol/ServiceImpl/ControlService.h>


#include <Paxos/PaxosConsensus.h>

#include "Captain.h"


namespace rong {

// 在主线程中最先初始化，所以不考虑竞争条件问题
Captain& Captain::instance() {
    static Captain service;
    return service;
}

Captain::Captain():
    initialized_(false){
}

using tzrpc::Dispatcher;
using tzrpc::Service;
using tzrpc::PaxosService;
using tzrpc::ClientService;
using tzrpc::ControlService;

bool Captain::init(const std::string& cfgFile) {

    if (initialized_) {
        roo::log_err("Captain already initlialized before ...");
        return false;
    }

    timer_ptr_ = std::make_shared<roo::Timer>();
    if (!timer_ptr_ || !timer_ptr_->init()) {
        roo::log_err("Create and init roo::Timer service failed.");
        return false;
    }

    setting_ptr_ = std::make_shared<roo::Setting>();
    if (!setting_ptr_ || !setting_ptr_->init(cfgFile)) {
        roo::log_err("Create and init roo::Setting with cfg %s failed.", cfgFile.c_str());
        return false;
    }

    auto setting_ptr = setting_ptr_->get_setting();
    if (!setting_ptr) {
        roo::log_err("roo::Setting return null pointer, maybe your conf file ill???");
        return false;
    }

    int log_level = 0;
    setting_ptr->lookupValue("log_level", log_level);
    if (log_level <= 0 || log_level > 7) {
        roo::log_warning("Invalid log_level %d, reset to default 7(DEBUG).", log_level);
        log_level = 7;
    }

    std::string log_path;
    setting_ptr->lookupValue("log_path", log_path);
    if(log_path.empty())
        log_path = "./log";

    roo::log_init(log_level, "", log_path, LOG_LOCAL6);
    roo::log_warning("Initialized roo::Log with level %d, path %s.", log_level, log_path.c_str());

    status_ptr_ = std::make_shared<roo::Status>();
    if (!status_ptr_) {
        roo::log_err("Create roo::Status failed.");
        return false;
    }
    
    net_server_ptr_ = std::make_shared<tzrpc::NetServer>("KanNetServer");
    if (!net_server_ptr_ || !net_server_ptr_->init()) {
        roo::log_err("init NetServer failed!");
        return false;
    }

    io_service_ptr_ = std::make_shared<roo::IoService>();
    if(!io_service_ptr_ || !io_service_ptr_->init()) {
        roo::log_err("create and initialize IoService failed.");
        return false;
    }


    // 先注册具体的服务
    // 这里的初始化是调用服务实现侧的初始化函数，意味着要完成配置读取等操作
    std::shared_ptr<Service> paxos_service = std::make_shared<PaxosService>("PaxosService");
    if (!paxos_service || !paxos_service->init()) {
        roo::log_err("create PaxosService failed.");
        return false;
    }
    Dispatcher::instance().register_service(tzrpc::ServiceID::PAXOS_SERVICE, paxos_service);

    std::shared_ptr<Service> client_service = std::make_shared<ClientService>("ClientService");
    if (!client_service || !client_service->init()) {
        roo::log_err("create ClientService failed.");
        return false;
    }
    Dispatcher::instance().register_service(tzrpc::ServiceID::CLIENT_SERVICE, client_service);

    std::shared_ptr<Service> control_service = std::make_shared<ControlService>("ControlService");
    if (!control_service || !control_service->init()) {
        roo::log_err("create ControlService failed.");
        return false;
    }
    Dispatcher::instance().register_service(tzrpc::ServiceID::CONTROL_SERVICE, control_service);


    // 再进行整体服务的初始化
    if (!Dispatcher::instance().init()) {
        roo::log_err("Init Dispatcher failed.");
        return false;
    }


    paxos_consensus_ptr_ = std::make_shared<PaxosConsensus>();
    if (!paxos_consensus_ptr_|| !paxos_consensus_ptr_->init()) {
        roo::log_err("create PaxosConsensus failed.");
        return false;
    }

    // do real service
    net_server_ptr_->service();

    roo::log_warning("Captain all initialized successfully.");
    initialized_ = true;

    return true;
}


bool Captain::service_graceful() {

    timer_ptr_->threads_join();
    net_server_ptr_->io_service_stop_graceful();
    return true;
}

void Captain::service_terminate() {
    ::sleep(1);
    ::_exit(0);
}

bool Captain::service_joinall() {

    timer_ptr_->threads_join();
    net_server_ptr_->io_service_join();
    return true;
}


} // end namespace rong
