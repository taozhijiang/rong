/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_CLOCK_H__
#define __PAXOS_CLOCK_H__

#include <xtra_rhel.h>
#include <chrono>

// DEPRECATED...
//
// 如果直接处理时间的话可能会比较抓狂，所以这边采用逻辑时钟的概念
// 来控制，即每 100ms定时器将当前的时钟值自增
// 内部涉及到的Raft的定时器都采用这个时钟来驱动


namespace rong {


#if __cplusplus >= 201103L
typedef std::chrono::steady_clock               steady_clock;
#else
typedef std::chrono::monotonic_clock            steady_clock;
#endif

typedef std::chrono::milliseconds               duration;
typedef std::chrono::time_point<steady_clock>   time_point;


// 简易用来进行心跳、选举等操作的定时器
class SimpleTimer {

public:
    explicit SimpleTimer() :
        enable_(false),
        immediate_(false),
        start_(steady_clock::now()) {
    }

    bool timeout(duration count_ms) {
        if (enable_ && immediate_) {
            immediate_ = false;
            return true;
        }

        if (enable_)
            return start_ + count_ms < steady_clock::now();

        return false;
    }

    // 是否在某个时间范围内，这边在投票优化的时候用到
    bool within(duration count_ms) {
        if (enable_ && immediate_) {
            immediate_ = false;
            return true;
        }

        if (enable_)
            return start_ + count_ms > steady_clock::now();

        return false;
    }

    // 启动定时器
    void schedule() {
        enable_ = true;
        start_ = steady_clock::now();
    }

    void disable() {
        enable_ = false;
    }

    void set_urgent() {
        immediate_ = true;
    }

private:
    bool enable_;

    // 用于立即触发事件发生，使用timeout或者within函数后，该标志会重置
    bool immediate_;
    time_point start_;
};

} // namespace rong


#endif // __PAXOS_CLOCK_H__
