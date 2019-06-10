/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <Paxos/LeaseProposer.h>
#include <Paxos/LeaseAcceptor.h>
#include <Paxos/LeaseLearner.h>

#include <Paxos/PaxosConsensus.h>

#include "MasterLease.h"

namespace rong {

MasterLease::MasterLease(PaxosConsensus& paxos_consensus) :
    acquireLease_(false),
    paxos_consensus_(paxos_consensus),
    option_(paxos_consensus.option_),
    proposer_(new LeaseProposer(*this)),
    acceptor_(new LeaseAcceptor(*this)),
    learner_(new LeaseLearner(*this)) {
}



bool MasterLease::init() {

    if (!proposer_ || !acceptor_ || !learner_)
        return false;

    // 主工作线程
    main_thread_ = std::thread(std::bind(&MasterLease::main_thread_loop, this));

    return true;
}

void MasterLease::acquire_lease() {
    acquireLease_ = true;
    proposer_->start_acquire_lease();
}

uint64_t MasterLease::id() const {
    return option_.id_;
}

int MasterLease::handle_paxos_lease(const Paxos::LeaseMessage& request, Paxos::LeaseMessage& response) {

    return -1;
}


void MasterLease::main_thread_loop() {

    while (!main_thread_stop_) {

        {
            std::unique_lock<std::mutex> lock(lease_mutex_);

            auto expire_tp = steady_clock::now() + std::chrono::milliseconds(100);
#if __cplusplus >= 201103L
            lease_notify_.wait_until(lock, expire_tp);
#else
            lease_notify_.wait_until(lock, expire_tp);
#endif
        }

        if (election_timer_.timeout(option_.master_lease_election_ms_)) {
            election_timer_.schedule();
        } else if (heartbeat_timer_.timeout(option_.master_lease_heartbeat_ms_)) {
            heartbeat_timer_.schedule();
        }

    }
}


} // namespace rong
