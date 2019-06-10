/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_LEASE_PROPOSER_H__
#define __PAXOS_LEASE_PROPOSER_H__

#include <xtra_rhel.h>

#include "MasterLease.h"
#include "LeaseState.h"

namespace rong {

class LeaseProposer {

public:
    explicit LeaseProposer(MasterLease& master_lease) :
        master_lease_(master_lease) {
        state_.init();
    }

    void start_acquire_lease() {

        master_lease_.heartbeat_timer_.disable();

        // 如果当前没有进行acquire，则进行之
        if (!(state_.preparing || state_.proposing))
            start_preparing();
    }

    void stop_acquire_lease() {
        state_.preparing = false;
        state_.proposing = false;

        master_lease_.heartbeat_timer_.disable();
        master_lease_.election_timer_.disable();
    }

    void start_preparing() {
        master_lease_.election_timer_.schedule();

        state_.proposing = false;
        state_.preparing = true;

        state_.leaseOwner = master_lease_.id();
        state_.highestReceivedProposalID = 0;
        //state_.proposalID =
    }

    void start_proposing() {

    }

private:


private:
    LeaseProposerState state_;
    MasterLease& master_lease_;
};


} // namespace rong

#endif // __PAXOS_LEASE_PROPOSER_H__
