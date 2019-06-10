/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_LEASE_LEARNER_H__
#define __PAXOS_LEASE_LEARNER_H__

#include <xtra_rhel.h>

#include "MasterLease.h"
#include "LeaseState.h"

namespace rong {

class LeaseLearner {

public:
    explicit LeaseLearner(MasterLease& master_lease):
        master_lease_(master_lease) {
        state_.init();
    }

private:

    LeaseLearnerState state_;
    MasterLease& master_lease_;
};


} // namespace rong

#endif // __PAXOS_LEASE_LEARNER_H__