/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __CAPTAIN_H__
#define __CAPTAIN_H__

#include <memory>
#include <string>
#include <map>
#include <vector>

namespace roo {
    class Setting;
    class Status;
    class Timer;
    class IoService;
}

namespace tzrpc {
    class NetServer;
}

namespace rong {


class PaxosConsensus;

class Captain {
public:
    static Captain& instance();

public:
    bool init(const std::string& cfgFile);

    bool service_joinall();
    bool service_graceful();
    void service_terminate();

private:
    Captain();

    ~Captain() {
        // Singleton should not destoried normally,
        // if happens, just terminate quickly
        ::exit(EXIT_SUCCESS);
    }


    bool initialized_;

public:

    std::shared_ptr<tzrpc::NetServer> net_server_ptr_;
    
    std::shared_ptr<roo::Setting> setting_ptr_;
    std::shared_ptr<roo::Status> status_ptr_;
    std::shared_ptr<roo::Timer> timer_ptr_;

    std::shared_ptr<roo::IoService> io_service_ptr_;

    std::shared_ptr<PaxosConsensus> paxos_consensus_ptr_;
};

} // end namespace rong


#endif //__CAPTAIN_H__
