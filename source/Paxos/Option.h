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
    duration    heartbeat_ms_;
    // 选举超时定时
    duration    election_timeout_ms_;
    // 优化，如果发现在选举超时时间内获得了其他Leader的信息，则拒绝此次投票
    duration    withhold_votes_ms_;

    // 在客户端等待响应的时候，Raft Leader在复制日志、等待状态机的超时时长
    duration    raft_distr_timeout_ms_;

    // 限制一次RPC中log能够传输的最大数量，0表示没有限制
    uint64_t    log_trans_count_;



    Option() :
        bootstrap_(false),
        id_(0),
        heartbeat_ms_(0),
        election_timeout_ms_(0),
        withhold_votes_ms_(0),
        raft_distr_timeout_ms_(0),
        log_trans_count_(0) { }

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

        if (heartbeat_ms_.count() == 0 || election_timeout_ms_.count() == 0 ||
            withhold_votes_ms_.count() == 0 ||
            heartbeat_ms_.count() >= election_timeout_ms_.count() ||
            heartbeat_ms_.count() <= 100 ||
            withhold_votes_ms_.count() <= 100 )
            return false;

        // 作为必选参数处理
        if (raft_distr_timeout_ms_.count() == 0)
                return false;
            
        return true;
    }


    std::string str() const {
        std::stringstream ss;
        ss << "Raft Configure:" << std::endl
            << "    id: " << id_ << std::endl
            << "    members: " << members_str_ << std::endl
            << "    log_path: " << log_path_ << std::endl
            << "    heartbeat_ms: " << heartbeat_ms_.count() << std::endl
            << "    election_timeout_ms: " << election_timeout_ms_.count() << std::endl
            << "    withhold_votes_ms: " << withhold_votes_ms_.count() << std::endl
            << "    raft_distr_timeout_ms: " << raft_distr_timeout_ms_.count() << std::endl
            << "    log_trans_count: " << log_trans_count_ << std::endl
        ;

        return ss.str();
    }
};



} // namespace rong

#endif // __PAXOS_OPTION_H__
