/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_CONTEXT_H__
#define __PAXOS_CONTEXT_H__

#include <xtra_rhel.h>

#include <Protocol/gen-cpp/PaxosMetadata.pb.h>
#include <Paxos/LogIf.h>

namespace rong {


// 需要更新访问meta数据
class LogIf;

class Context {

    typedef PaxosMetadata::Metadata MetaDataType;

public:
    explicit Context(uint64_t id, std::unique_ptr<LogIf>& log_meta);
    ~Context() = default;

public:

    bool init(const MetaDataType& meta);

    // 获取全局单调递增的proposalID序列号
    uint64_t next_proposal_id(uint64_t hint) const;

    bool startup_instance(uint64_t& curr_instance_id);
    void close_instance() { active_ = false; }
    uint64_t highest_instance_id() const { return highest_instance_id_; }
    void update_highest_instance_id(uint64_t instance_id);

    std::string str() const;

public:
    // 当前主机的NodeID
    const uint64_t kID;
    std::unique_ptr<LogIf>& log_meta_;

private:

    // 全局的RoundID，需要持久化
    uint64_t highest_instance_id_;

    // 每次系统启动后自增，用于构造悠久唯一递增的Proposal
    uint64_t restart_counter_;

    // 需要使用startup_instance()来开启，使用close_instance()来关闭
    bool active_;
};

} // namespace rong

#endif // __PAXOS_CONTEXT_H__

