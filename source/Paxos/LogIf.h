/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <xtra_rhel.h>

#include <cinttypes>
#include <memory>
#include <vector>

#include <Protocol/gen-cpp/Paxos.pb.h>
#include <Protocol/gen-cpp/PaxosMetadata.pb.h>

#ifndef __PAXOS_LOGIF_H__
#define __PAXOS_LOGIF_H__

// 和Raft不一样，这里的Log实际是Learn(提交)的日志，而不是
// 本地化的日志(这种情况当新选主之后，某些日志可能会被改写)

namespace rong {


extern const char* const META_RESTART_COUNTER;
extern const char* const META_HIGHEST_INSTANCE_ID;

extern const char* const META_INSTANCE_ID;
extern const char* const META_ACCEPTED;
extern const char* const META_PROMISED_PROPOSAL_ID;
extern const char* const META_ACCEPTED_PROPOSAL_ID;
extern const char* const META_ACCEPTED_VALUE;

extern const char* const META_APPLY_INSTANCE_ID;

class LogIf {

    __noncopyable__(LogIf)

public:
    typedef rong::Paxos::Entry                Entry;
    typedef rong::PaxosMetadata::Metadata     LogMeta;

    typedef std::shared_ptr<Entry>            EntryPtr;

    LogIf() = default;
    virtual ~LogIf() = default;

    // 这里不是严格意义上的追加，而是应该再accept成功之后再添加指定instance_id索引位置的日志
    // 如果成功则返回值和传递的paxosId是一致的，否则就返回last_index
    virtual uint64_t append(uint64_t index, const EntryPtr& newEntry) = 0;

    virtual EntryPtr entry(uint64_t index) const = 0;
    virtual bool     entries(uint64_t start, std::vector<EntryPtr>& entries, uint64_t limit = 0) const = 0;

    virtual uint64_t start_index() const = 0;
    virtual uint64_t last_index() const = 0;

    // 截取日志
    virtual void truncate_prefix(uint64_t start_index) = 0;
    virtual void truncate_suffix(uint64_t last_index) = 0;

    virtual int meta_data(LogMeta* meta_data) const = 0;
    virtual int set_meta_data(const LogMeta& meta) const = 0;

    virtual uint64_t meta_apply_instance_id() const = 0;
    virtual int set_meta_apply_instance_id(uint64_t instance_id) const = 0;

    virtual uint64_t meta_highest_instance_id() const = 0;
    virtual int set_meta_highest_instance_id(uint64_t instance_id) const = 0;
};


} // end namespace rong

#endif // __PAXOS_LOGIF_H__
