/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_OPTION_H__
#define __PAXOS_OPTION_H__

#include <xtra_rhel.h>
#include <other/Log.h>

#include <Paxos/Clock.h>

// Raft相关的配置参数

namespace rong {

struct Option {

    // 最初集群起来的时候，增加一个空日志，模拟一个已经存在的日志项
    bool        bootstrap_;

    uint64_t    id_;
    std::string log_path_;

    std::string members_str_;
    std::map<uint64_t, std::pair<std::string, uint16_t>> members_;

    // 心跳发送周期
    duration    master_lease_election_ms_;    // 选取超时，如果超过这个时间重新选主
    duration    master_lease_heartbeat_ms_;   // master主动续约时间，类似于心跳时间
    duration    prepare_propose_timeout_ms_;


    Option() :
        bootstrap_(false),
        id_(0),
        master_lease_election_ms_(0),
        master_lease_heartbeat_ms_(0) { }

    ~Option() = default;

    // 检查配置的合法性
    bool validate() const {
        // 1 + n
        if (members_str_.empty() || members_.size() < 2)
            return false;

        if (id_ == 0)
            return false;

        if (log_path_.empty())
            return false;

        if (master_lease_election_ms_.count() == 0 ||
            master_lease_heartbeat_ms_.count() == 0 ||
            prepare_propose_timeout_ms_.count() == 0 ||
            master_lease_election_ms_.count() <= master_lease_heartbeat_ms_.count())
            return false;

        return true;
    }


    std::string str() const {
        std::stringstream ss;
        ss << "Raft Configure:" << std::endl
            << "    id: " << id_ << std::endl
            << "    members: " << members_str_ << std::endl
            << "    log_path: " << log_path_ << std::endl
            << "    master_lease_election_ms: " << master_lease_election_ms_.count() << std::endl
            << "    master_lease_heartbeat_ms: " << master_lease_heartbeat_ms_.count() << std::endl
            << "    prepare_propose_timeout_ms: " << prepare_propose_timeout_ms_.count() << std::endl
        ;

        return ss.str();
    }
};



} // namespace rong

#endif // __PAXOS_OPTION_H__
