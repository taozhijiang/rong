/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_BASIC_LEARNER_H__
#define __PAXOS_BASIC_LEARNER_H__

#include <xtra_rhel.h>
#include <other/Log.h>

#include <Protocol/gen-cpp/Paxos.pb.h>

#include <Paxos/PaxosConsensus.h>
#include "BasicState.h"

namespace rong {

class BasicLearner {

public:
    BasicLearner(PaxosConsensus& paxos_consensus) :
        paxos_consensus_(paxos_consensus) {
        state_.init();
    }



    BasicLearnerState& state() {
        return state_;
    }

private:
    BasicLearnerState state_;
    PaxosConsensus& paxos_consensus_;
};


} // namespace rong

#endif // __PAXOS_BASIC_LEARNER_H__
