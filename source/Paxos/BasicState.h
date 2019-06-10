/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_BASIC_STATE_H__
#define __PAXOS_BASIC_STATE_H__

#include <xtra_rhel.h>


namespace rong {

struct BasicProposerState {

    bool active() {
        return (preparing || proposing);
    }

    void init() {
        preparing =    false;
        proposing =    false;
        proposalID = 0;
        highestReceivedProposalID =    0;
        highestPromisedProposalID = 0;
        value.clear();
        leader = false;
        numProposals = 0;
    }

    // member
    bool        preparing;
    bool        proposing;
    uint64_t    proposalID;
    uint64_t    highestReceivedProposalID;
    uint64_t    highestPromisedProposalID;
    std::string value;
    bool        leader;          // multi paxos
    unsigned    numProposals;    // number of proposal runs in this Paxos round
};

struct BasicAcceptorState {

    void init() {
        promisedProposalID = 0;
        acceptedProposalID = 0;
        accepted = false;
        acceptedValue.clear();
    }

    // member
    uint64_t    promisedProposalID;
    bool        accepted;
    uint64_t    acceptedProposalID;
    std::string acceptedValue;
};

struct BasicLearnerState {

    void init() {
        learned = 0;
        value.clear();
    }

// member
    bool        learned;
    std::string value;
};


} // namespace rong

#endif // __PAXOS_BASIC_STATE_H__
