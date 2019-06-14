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

        if (request.type() == Paxos::kBProposeLearnValue ) {
            return on_learn_value_request(request, response);
        } else if (request.type() == Paxos::kBProposeChosenValue ) {
            return on_chosen_value_request(request, response);
        }

        roo::log_err("Unhandle Learn request type: %d.", request.type());
    }


    void on_learn_response(const Paxos::BasicMessage& response) {

        roo::log_info("Current local last_index %lu, request.instance_id %lu.",
                      log_meta_->last_index(), response.instance_id());

        if (response.type() == Paxos::kBProposeLearnResponse ) {
            return on_learn_value_response(response);
        } else if (response.type() == Paxos::kBProposeChosenResponse ) {
            return on_chosen_value_response(response);
        }

        roo::log_err("Unhandle Learn response type: %d.", response.type());
    }

    BasicLearnerState& state() {
        return state_;
    }

private:

    // 对端主动向自己发送学习日志
    void on_learn_value_request(const Paxos::BasicMessage& request, Paxos::BasicMessage& response) {

        response.set_type(Paxos::kBProposeLearnResponse);
        response.set_node_id(paxos_consensus_.context_->kID);
        response.set_proposal_id(request.proposal_id());

        do {

            // 跳过，已经存在的日志条目无需添加
            if (request.instance_id() <= log_meta_->last_index())
                break;

            if (request.instance_id() > log_meta_->last_index() + 1) {
                roo::log_err("LearnLog Gap found, reject with last_index %lu.", log_meta_->last_index());
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

        // 任何情况下，只要收到on_learn_value_request，那么表示本轮的value已经Chosen了，
        // 那么就应该使用close_instance()来关闭本轮的instance
        //
        // close this instance
        paxos_consensus_.close_instance();

        // 响应是最后一条日志索引
        response.set_instance_id(log_meta_->last_index());
        return;
    }

    // 检查本地日志是否存在，如果存在则发送给对端
    void on_chosen_value_request(const Paxos::BasicMessage& request, Paxos::BasicMessage& response) {

        response.set_type(Paxos::kBProposeChosenResponse);
        response.set_node_id(paxos_consensus_.context_->kID);
        response.set_proposal_id(request.proposal_id());

        do {

            // 日志不存在
            if (request.instance_id() > log_meta_->last_index()) {
                roo::log_err("request log at %lu not found, current last_index %lu.",
                             request.instance_id(), log_meta_->last_index());
                response.set_instance_id(log_meta_->last_index());
                break;
            }

            // do add the log
            auto entry = log_meta_->entry(request.instance_id());
            if (!entry) {
                PANIC("request log at %lu not found!", request.instance_id());
            }

            response.set_instance_id(request.instance_id());
            response.set_value(entry->data());
            response.set_log_last_index(log_meta_->last_index());

        } while (0);

        return;
    }


    // 如果发现发送给对端的learn_value没能被学习，即得到的响应日志的
    // 索引在last_index之前，这边只是打印，让对端发送RequestChosen来主动学习
    void on_learn_value_response(const Paxos::BasicMessage& response) {
        if (response.instance_id() < log_meta_->last_index())
            roo::log_warning("Peer %lu log at %lu, current last_index %lu, should request_chonsen.",
                             response.node_id(), response.instance_id(),
                             log_meta_->last_index());
    }

    void on_chosen_value_response(const Paxos::BasicMessage& response) {

        do {

            // 跳过，已经存在的日志条目无需添加
            if (response.instance_id() <= log_meta_->last_index())
                break;

            // 一条一条发送接收
            if (response.instance_id() > log_meta_->last_index() + 1) {
                PANIC("LearnLog Gap found, should not meet in chosen response %lu.", log_meta_->last_index());
                break;
            }

            // do add the log
            auto entry = std::make_shared<LogIf::Entry>();
            if (!entry) {
                roo::log_err("Create new entry failed.");
                break;
            }

            entry->set_type(Paxos::EntryType::kNormal);
            entry->set_data(response.value());

            log_meta_->append(response.instance_id(), entry);
            paxos_consensus_.state_machine_notify();

        } while (0);
    }

private:

    BasicLearnerState state_;

    PaxosConsensus& paxos_consensus_;
    std::unique_ptr<LogIf>& log_meta_;
};


} // namespace rong

#endif // __PAXOS_BASIC_LEARNER_H__
