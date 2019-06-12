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
#include <Paxos/LogIf.h>
#include "BasicState.h"

namespace rong {

class BasicLearner {

public:
    BasicLearner(PaxosConsensus& paxos_consensus, std::unique_ptr<LogIf>& log_meta) :
        paxos_consensus_(paxos_consensus),
        log_meta_(log_meta) {
        state_.init();
    }

    void on_learn_request(const Paxos::BasicMessage& request, Paxos::BasicMessage& response) {
#if 0
        roo::log_info("current request proposal_id: %lu", request.proposal_id());

        response.set_type(Paxos::kBProposeRequestChosen);
        response.set_node_id(paxos_consensus_.context_->kID);
        response.set_proposal_id(request.proposal_id());
        response.set_instance_id(request.instance_id());

        // 发送的日志比本地日志多，则不处理
        if (request.instance_id() <= paxos_consensus_.instance_id()) {
            return;
        }

        // 发送的日志和本地日志刚好匹配，追加返回
        if (request.instance_id() > paxos_consensus_.instance_id() + 1) {

        }

        // 发送的日志比本地日志多，中间有日志间隙，删除之
        response.set_instance_id(paxos_consensus_.instance_id());

        if (request.proposal_id() < state_.promisedProposalID) {
            response.set_type(Paxos::kBProposeRejected);
            return;
        }

        state_.accepted = true;
        state_.acceptedProposalID = request.proposal_id();
        state_.acceptedValue = request.value();

        response.set_type(Paxos::kBProposeAccepted);
#endif
    }

    BasicLearnerState& state() {
        return state_;
    }

private:

    BasicLearnerState state_;

    PaxosConsensus& paxos_consensus_;
    std::unique_ptr<LogIf>& log_meta_;
};


} // namespace rong

#endif // __PAXOS_BASIC_LEARNER_H__
