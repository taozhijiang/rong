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

        roo::log_info("Current local last_index %lu, request.instance_id %lu.",
                      log_meta_->last_index(), request.instance_id());

        response.set_type(Paxos::kBProposeRequestChosen);
        response.set_node_id(paxos_consensus_.context_->kID);
        response.set_proposal_id(request.proposal_id());

        do {

            // 跳过，无需添加
            if (request.instance_id() <= log_meta_->last_index())
                break;

            if (request.instance_id() > log_meta_->last_index() + 1) {
                roo::log_err("LearnLog Gap found, reject with %lu.", log_meta_->last_index());
                break;
            }

            // do add the log
            auto entry = std::make_shared<LogIf::Entry>();
            if (!entry) {
                roo::log_err("Create new entry failed.");
                break;
            }

            entry->set_type(Paxos::EntryType::kNormal);
            entry->set_data(request.value());

            log_meta_->append(request.instance_id(), entry);
            paxos_consensus_.state_machine_notify();

        } while (0);

        // 任何情况下，只要收到on_learn_request，那么表示本轮的value已经Chosen了，
        // 那么就应该使用close_instance()来关闭本轮的instance
        //
        // close this instance
        paxos_consensus_.close_instance();

        response.set_instance_id(log_meta_->last_index());
        return;
    }


    void on_learn_response(const Paxos::BasicMessage& response) {

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
