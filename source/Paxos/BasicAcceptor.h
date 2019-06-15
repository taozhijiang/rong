/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_BASIC_ACCEPTOR_H__
#define __PAXOS_BASIC_ACCEPTOR_H__

#include <xtra_rhel.h>
#include <other/Log.h>

#include <Protocol/gen-cpp/Paxos.pb.h>
#include <Protocol/gen-cpp/PaxosMetadata.pb.h>

#include <Paxos/PaxosConsensus.h>
#include "BasicState.h"


namespace rong {

class BasicAcceptor {

    typedef PaxosMetadata::Metadata MetaDataType;

public:
    BasicAcceptor(PaxosConsensus& paxos_consensus, const MetaDataType& meta) :
        paxos_consensus_(paxos_consensus) {
        state_.init();

        // 崩溃重启，加载持久化信息
        if (meta.has_accepted())
            state_.accepted = meta.accepted();
        if (meta.has_promised_proposal_id())
            state_.promisedProposalID = meta.promised_proposal_id();
        if (meta.has_accepted_proposal_id())
            state_.acceptedProposalID = meta.accepted_proposal_id();
        if (meta.has_accepted_value())
            state_.acceptedValue = meta.accepted_value();
    }


    void on_prepare_request(const Paxos::BasicMessage& request, Paxos::BasicMessage& response) {

        roo::log_info("Current local instance_id %lu, promised_proposal_id %lu, "
                      "        request instance_id %lu, proposal_id: %lu.",
                      paxos_consensus_.current_instance_id(), state_.promisedProposalID,
                      request.instance_id(), request.proposal_id());

        response.set_node_id(paxos_consensus_.context_->kID);
        response.set_proposal_id(request.proposal_id());
        response.set_instance_id(request.instance_id());
        response.set_log_last_index(paxos_consensus_.log_meta_->last_index());


        // 过小的proposalID，拒绝之
        if (request.proposal_id() < state_.promisedProposalID) {
            response.set_type(Paxos::kBPrepareRejected);
            response.set_promised_proposal_id(state_.promisedProposalID);

            return;
        }

        // 保存promisedProposalID
        state_.promisedProposalID = request.proposal_id();

        if (!state_.accepted) {
            response.set_type(Paxos::kBPrepareCurrentlyOpen);
        } else {
            roo::log_warning("for instance_id %lu, previous accepted with proposal_id %lu, value size %lu.",
                             request.instance_id(), state_.acceptedProposalID, state_.acceptedValue.size());
            response.set_type(Paxos::kBPreparePreviouslyAccepted);
            response.set_accepted_proposal_id(state_.acceptedProposalID);
            response.set_value(state_.acceptedValue);
        }

        // 状态持久化，然后再响应请求
        persist_state();
    }

    void on_propose_request(const Paxos::BasicMessage& request, Paxos::BasicMessage& response) {

        roo::log_info("Current local instance_id %lu, promised_proposal_id %lu, "
                      "        request instance_id %lu, proposal_id: %lu.",
                      paxos_consensus_.current_instance_id(), state_.promisedProposalID,
                      request.instance_id(), request.proposal_id());

        response.set_node_id(paxos_consensus_.context_->kID);
        response.set_proposal_id(request.proposal_id());
        response.set_instance_id(request.instance_id());
        response.set_log_last_index(paxos_consensus_.log_meta_->last_index());

        if (request.proposal_id() < state_.promisedProposalID) {
            response.set_type(Paxos::kBProposeRejected);
            return;
        }

        state_.accepted = true;
        state_.acceptedProposalID = request.proposal_id();
        state_.acceptedValue = request.value();

        response.set_type(Paxos::kBProposeAccepted);

        // 状态持久化，然后再响应请求
        persist_state();
    }


    BasicAcceptorState& state() {
        return state_;
    }

private:

    void persist_state() {

        MetaDataType meta {};
        meta.set_instance_id(paxos_consensus_.current_instance_id());
        meta.set_accepted(state_.accepted);
        meta.set_promised_proposal_id(state_.promisedProposalID);
        meta.set_accepted_proposal_id(state_.acceptedProposalID);
        meta.set_accepted_value(state_.acceptedValue);

        paxos_consensus_.log_meta_->set_meta_data(meta);
    }

    BasicAcceptorState state_;
    PaxosConsensus& paxos_consensus_;
};


} // namespace rong

#endif // __PAXOS_BASIC_ACCEPTOR_H__
