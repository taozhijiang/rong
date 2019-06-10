/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_LEASE_ACCEPTOR_H__
#define __PAXOS_LEASE_ACCEPTOR_H__

#include <xtra_rhel.h>

#include "MasterLease.h"
#include "LeaseState.h"

namespace rong {

class LeaseAcceptor {

public:
    explicit LeaseAcceptor(MasterLease& master_lease):
        highestProposalID_(0),
        master_lease_(master_lease) {
        state_.init();
    }

private:

    uint64_t highestProposalID_;

    LeaseAcceptorState state_;
    MasterLease& master_lease_;
};


} // namespace rong

#endif // __PAXOS_LEASE_ACCEPTOR_H__