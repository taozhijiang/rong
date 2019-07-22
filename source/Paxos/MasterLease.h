/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_MASTER_LEASE_H__
#define __PAXOS_MASTER_LEASE_H__

#include <xtra_rhel.h>
#include <thread>
#include <mutex>
#include <condition_variable> 

#include <Paxos/Clock.h>
#include <Paxos/Option.h>

#include <Protocol/gen-cpp/Paxos.pb.h>

namespace rong {

class PaxosConsensus;

class LeaseProposer;
class LeaseAcceptor;
class LeaseLearner;

class MasterLease {

    friend class LeaseProposer;
    friend class LeaseAcceptor;
    friend class LeaseLearner;

public:
    MasterLease(PaxosConsensus& paxos_consensus);
    ~MasterLease() = default;

    bool init();
    void acquire_lease();
    uint64_t id() const;

    int handle_paxos_lease(const Paxos::LeaseMessage& request, Paxos::LeaseMessage& response);

private:

    bool acquireLease_;
    PaxosConsensus& paxos_consensus_;
    Option option_;

    std::shared_ptr<LeaseProposer> proposer_;
    std::shared_ptr<LeaseAcceptor> acceptor_;
    std::shared_ptr<LeaseLearner>  learner_;

    // Timer
    SimpleTimer heartbeat_timer_;
    SimpleTimer election_timer_;

    std::mutex lease_mutex_;
    std::condition_variable lease_notify_;

    bool main_thread_stop_;
    std::thread main_thread_;
    void main_thread_loop();
};


} // namespace rong

#endif // __PAXOS_MASTER_LEASE_H__
