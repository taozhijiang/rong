/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <sstream>
#include <iostream>

#include <message/ProtoBuf.h>
#include <RpcClient.h>
#include <Protocol/Common.h>
#include <Protocol/gen-cpp/Client.pb.h>

static void usage() {
    std::stringstream ss;

    ss << std::endl
        << "./clientOps get  thread_num      " << std::endl
        << "            set  thread_num      " << std::endl
        << std::endl;

    std::cout << ss.str();

    ::exit(EXIT_SUCCESS);
}


const std::string srv_addr = "127.0.0.1";
const uint16_t    srv_port = 10802;

using namespace tzrpc_client;


volatile bool start = false;
volatile bool stop  = false;

time_t            start_time = 0;
volatile uint64_t count = 0;

std::string       key = "perf_test_key";

static std::string generate_random_str() {

    std::stringstream ss;
    ss << "message with random [" << ::random() << "] include";
    return ss.str();
}

void* perf_get_run(void* x_void_ptr) {

    while(!start)
        ::usleep(1);

    RpcClient client(srv_addr, srv_port);

    while(!stop) {

        std::string mar_str;
        rong::Client::StateMachineSelectOps::Request request;
        request.mutable_read()->set_key(key);
        if (!roo::ProtoBuf::marshalling_to_string(request, &mar_str)) {
            std::cerr << "marshalling message failed." << std::endl;
            stop = true;
            continue;
        }

        std::string resp_str;
        auto status = client.call_RPC(tzrpc::ServiceID::CLIENT_SERVICE,
                                      rong::Client::OpCode::kSelect,
                                      mar_str, resp_str);
        if (status != RpcClientStatus::OK) {
            std::cerr << "call failed, return code [" << static_cast<uint8_t>(status) << "]" << std::endl;
            stop = true;
            continue;
        }

        rong::Client::StateMachineSelectOps::Response response;
        if (!roo::ProtoBuf::unmarshalling_from_string(resp_str, &response)) {
            std::cerr << "unmarshalling message failed." << std::endl;
            stop = true;
            continue;
        }

        if (!response.has_code() || response.code() != 0) {
            std::cerr << "response code check error" << std::endl;
            stop = true;
            continue;
        }

        // increment success case
        ++ count;
    }
}

void* perf_set_run(void* x_void_ptr) {

    while(!start)
        ::usleep(1);

    RpcClient client(srv_addr, srv_port);

    while(!stop) {

        std::string mar_str;
        rong::Client::StateMachineUpdateOps::Request request;
        request.mutable_write()->set_key(key);
        request.mutable_write()->set_content(generate_random_str());
        if (!roo::ProtoBuf::marshalling_to_string(request, &mar_str)) {
            std::cerr << "marshalling message failed." << std::endl;
            stop = true;
            continue;
        }

        std::string resp_str;
        auto status = client.call_RPC(tzrpc::ServiceID::CLIENT_SERVICE,
                                      rong::Client::OpCode::kUpdate,
                                      mar_str, resp_str);
        if (status != RpcClientStatus::OK) {
            std::cerr << "call failed, return code [" << static_cast<uint8_t>(status) << "]" << std::endl;
            stop = true;
            continue;
        }

        rong::Client::StateMachineUpdateOps::Response response;
        if (!roo::ProtoBuf::unmarshalling_from_string(resp_str, &response)) {
            std::cerr << "unmarshalling message failed." << std::endl;
            stop = true;
            continue;
        }

        if (!response.has_code() || response.code() != 0) {
            std::cerr << "response code check error" << std::endl;
            stop = true;
            continue;
        }

        // increment success case
        ++ count;
    }

    return NULL;
}

int main(int argc, char* argv[]) {

    if (argc < 3)
        usage();
    if (strncmp(argv[1], "get", 3) != 0 && strncmp(argv[1], "set", 3) != 0)
        usage();

    std::string cmd = std::string(argv[1]);
    int thread_num  = ::atoi(argv[2]);

    std::vector<pthread_t> tids( thread_num,  0);

    for(size_t i=0; i<tids.size(); ++i) {
        
        if (cmd == "get") 
            pthread_create(&tids[i], NULL, perf_get_run, NULL);
        else
            pthread_create(&tids[i], NULL, perf_set_run, NULL);

        std::cerr << "starting thread with id: " << tids[i] << std::endl;
    }

    ::sleep(3);
    std::cerr << "begin to test, press any to stop." << std::endl;
    start_time = ::time(NULL);
    start = true;

    int ch = getchar();
    stop = true;
    time_t stop_time = ::time(NULL);

    uint64_t count_per_sec = count / ( stop_time - start_time);
    fprintf(stderr, "total count %ld, time: %ld, perf: %ld tps\n", count, stop_time - start_time, count_per_sec);

    for(size_t i=0; i<tids.size(); ++i) {
        pthread_join(tids[i], NULL);
        std::cerr<< "joining " << tids[i] << std::endl;
    }

    std::cerr << "done" << std::endl;

    return 0;
}
