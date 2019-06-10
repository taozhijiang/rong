/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_BASIC_PROPOSER_H__
#define __PAXOS_BASIC_PROPOSER_H__

#include <xtra_rhel.h>
#include <other/Log.h>

#include <Protocol/gen-cpp/Paxos.pb.h>

#include <Paxos/PaxosConsensus.h>
#include "BasicState.h"

namespace rong {

class BasicProposer {

public:
    BasicProposer(PaxosConsensus& paxos_consensus) :
        paxos_consensus_(paxos_consensus) {
        state_.init();
    }

    // 接收客户端发起提案请求
    bool propose(const std::string& value) {

        state_.value = value;

#if 0
        // leader, 首次提起
        if(state_.leader && state_.numProposals == 0) {
            state_.numProposals ++;
            start_proposing();
        } else {
            start_preparing();
        }
#endif
        start_preparing();
        return true;
    }

    void start_preparing() {

        state_.preparing = true;
        state_.proposing = false;
        state_.numProposals++;
        state_.proposalID = paxos_consensus_.next_proposal_id(
            std::max(state_.proposalID, state_.highestPromisedProposalID));
        state_.highestReceivedProposalID = 0;

        Paxos::BasicMessage message;
        message.set_type(Paxos::BasicMessageType::kBPrepareRequest);
        message.set_node_id(paxos_consensus_.context_->kID);
        message.set_proposal_id(state_.proposalID);
        message.set_instance_id(paxos_consensus_.instance_id());

        granted_.clear();
        rejected_.clear();
        granted_.insert(paxos_consensus_.context_->kID);

        paxos_consensus_.send_paxos_basic(message);
    }


    void start_proposing() {

        state_.preparing = false;
        state_.proposing = true;

        Paxos::BasicMessage message;
        message.set_type(Paxos::BasicMessageType::kBProposeRequest);
        message.set_node_id(paxos_consensus_.context_->kID);
        message.set_proposal_id(state_.proposalID);
        message.set_instance_id(paxos_consensus_.instance_id());
        message.set_value(state_.value);

        granted_.clear();
        rejected_.clear();
        granted_.insert(paxos_consensus_.context_->kID);

        paxos_consensus_.send_paxos_basic(message);
    }

    void on_prepare_response(const Paxos::BasicMessage& response) {

        if (!state_.preparing || response.proposal_id() != state_.proposalID) {
            roo::log_err("Invalid state detected, preparing %d, expected proposalID %lu get %lu",
                         state_.preparing, state_.proposalID, response.proposal_id());
            return;
        }

        if (response.type() == Paxos::kBPrepareRejected) {
            if (response.promised_proposal_id() > state_.highestPromisedProposalID) {
                roo::log_info("Rejected with new highestPromisedProposalID %lu.",
                              response.promised_proposal_id());
                state_.highestPromisedProposalID = response.promised_proposal_id();
                rejected_.insert(response.node_id());
            }
        } else if (response.type() == Paxos::kBPreparePreviouslyAccepted &&
                   response.accepted_proposal_id() >= state_.highestReceivedProposalID) {
            /* the >= could be replaced by > which would result in less copys
             * however this would result in complications in multi paxos
             * in the multi paxos steady state this branch is inactive
             * it only runs after leader failure
             * so it's ok
             */

            state_.highestReceivedProposalID = response.accepted_proposal_id();
            state_.value = response.value();
            granted_.insert(response.node_id());
        } else if (response.type() == Paxos::kBPrepareCurrentlyOpen) {
            granted_.insert(response.node_id());
        } else {
            roo::log_err("Unhandled message type: %d", response.type());
            return;
        }

        if (granted_.size() >= paxos_consensus_.quorum_count()) {
            start_proposing();
            return;
        }

        if (rejected_.size() >= paxos_consensus_.quorum_count()) {
            start_preparing();
            return;
        }
    }


    void on_propose_response(const Paxos::BasicMessage& response) {

        if (!state_.proposing || response.proposal_id() != state_.proposalID) {
            roo::log_err("Invalid state detected, proposing %d, expected proposalID %lu get %lu",
                         state_.proposing, state_.proposalID, response.proposal_id());
            return;
        }

        if (response.type() == Paxos::kBProposeAccepted) {
            granted_.insert(response.node_id());
        } else if (response.type() == Paxos::kBProposeRejected) {
            rejected_.insert(response.node_id());
        }

        if (granted_.size() >= paxos_consensus_.quorum_count()) {
            roo::log_warning("Good, value chosen at instance_id %lu, proposalID %lu,"
                             "value size %lu.",
                             paxos_consensus_.instance_id(), state_.proposalID, state_.value.size());

            // Broadcast learn message to all nodes.

            // PaxosConsensus should close this instance.
            return;
        }

        if (rejected_.size() >= paxos_consensus_.quorum_count()) {
            start_preparing();
            return;
        }
    }

    BasicProposerState& state() {
        return state_;
    }

private:

    // keeping track of messages during prepare and propose phases
    std::set<uint64_t> granted_;
    std::set<uint64_t> rejected_;

    BasicProposerState state_;
    PaxosConsensus& paxos_consensus_;
};


} // namespace rong

#endif // __PAXOS_BASIC_PROPOSER_H__
