/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <other/Log.h>

#include <Paxos/Context.h>
#include <Paxos/Clock.h>


namespace rong {


Context::Context(uint64_t id, std::unique_ptr<LogIf>& log_meta) :
    kID(id),
    log_meta_(log_meta),
    highest_instance_id_(0),
    restart_counter_(0),
    active_(false) {
}

bool Context::init(const MetaDataType& meta) {

    if (meta.has_highest_instance_id())
        highest_instance_id_ = meta.highest_instance_id();
    if (meta.has_restart_counter())
        restart_counter_ = meta.restart_counter();

    if (meta.has_instance_id() && highest_instance_id_ < meta.instance_id()) {
        highest_instance_id_ = meta.instance_id();
    }

    roo::log_info("Full Context info: \n%s", this->str().c_str());

    MetaDataType n_meta;
    n_meta.set_restart_counter(restart_counter_ + 1);
    n_meta.set_highest_instance_id(highest_instance_id_);
    log_meta_->set_meta_data(n_meta);

    return true;
}

bool Context::startup_instance(uint64_t& curr_instance_id) {
    if (active_)
        return false;

    active_ = true;
    curr_instance_id = ++highest_instance_id_;

    log_meta_->set_meta_highest_instance_id(curr_instance_id);
    return true;
}

void Context::update_highest_instance_id(uint64_t instance_id) {
    if (instance_id > highest_instance_id_) {
        highest_instance_id_ = instance_id;
        log_meta_->set_meta_highest_instance_id(instance_id);
    }
}

// ProposeID需要保证全区唯一，而且是单调递增的，设计方法为
// couter, restart_counter, node_id
uint64_t Context::next_proposal_id(uint64_t hint) const {

    static const uint32_t kNodeWidth = 8;
    static const uint32_t kRestartCounterWidth = 16;

    uint64_t left, middle, right;

    left = hint >> (kNodeWidth + kRestartCounterWidth);
    left++;
    left = left << (kNodeWidth + kRestartCounterWidth);

    middle = restart_counter_ << kRestartCounterWidth;
    right  = kID;

    uint64_t next = left | middle | right;
    return next;
}


std::string Context::str() const {

    std::stringstream ss;

    ss  << "   server_id: " << kID << std::endl
        << "   highest_instance_id: " << highest_instance_id_ << std::endl
        << "   restart_counter: " << restart_counter_ << std::endl
    ;

    return ss.str();
}

} // namespace rong
