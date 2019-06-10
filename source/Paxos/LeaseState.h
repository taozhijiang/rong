/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_LEASE_STATE_H__
#define __PAXOS_LEASE_STATE_H__

#include <xtra_rhel.h>


namespace rong {

struct LeaseProposerState {

    bool active() const {
        return (preparing || proposing);
    }
    
    void init() {
        preparing   = false;
        proposing   = false;
        proposalID  = 0;
        highestReceivedProposalID = 0;
        expireTime  = 0;
    }
    
    // member
    bool        preparing;
    bool        proposing;
    uint64_t    proposalID;
    uint64_t    highestReceivedProposalID;
    unsigned    leaseOwner;
    uint64_t    expireTime;

};

struct LeaseAcceptorState {

    void init() {
        promisedProposalID = 0;

        accepted = false;
        acceptedProposalID = 0;
        acceptedLeaseOwner = 0;
        acceptedDuration = 0;
        acceptedExpireTime = 0;
    }

    // called by OnLeaseTimeout
    void clear() {
        accepted = false;
        acceptedProposalID = 0;
        acceptedLeaseOwner = 0;
        acceptedDuration   = 0;
        acceptedExpireTime = 0;
    }

    // member
    uint64_t    promisedProposalID;
    bool        accepted;
    uint64_t    acceptedProposalID;
    unsigned    acceptedLeaseOwner;
    unsigned    acceptedDuration;
    uint64_t    acceptedExpireTime;
};

struct LeaseLearnerState {

    void init() {
        learned = 0;
        leaseOwner = 0;
        expireTime = 0;
        leaseEpoch = 0;
    }
    
    // called by OnLeaseTimeout
    void clear() {
        learned = 0;
        leaseOwner = 0;
        expireTime = 0;
        leaseEpoch++;
    }
    
    // member
    bool        learned;
    unsigned    leaseOwner;
    uint64_t    expireTime;
    uint64_t    leaseEpoch;
};


} // namespace rong

#endif // __PAXOS_LEASE_STATE_H__