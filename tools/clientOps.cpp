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
        << "./clientOps get <key>      " << std::endl
        << "            set <key> <val>" << std::endl
        << "            keys           " << std::endl
        << "            del <key>      " << std::endl
        << std::endl;

    std::cout << ss.str();

    ::exit(EXIT_SUCCESS);
}


const std::string srv_addr = "127.0.0.1";
const uint16_t    srv_port = 10902;

using namespace tzrpc_client;

int main(int argc, char* argv[]) {

    if (argc < 2)
        usage();

    RpcClient client(srv_addr, srv_port);

    if (::strncmp(argv[1], "get", 3) == 0) {

        std::string key = argv[2];
        std::string mar_str;
        rong::Client::StateMachineSelectOps::Request request;
        request.mutable_read()->set_key(key);
        if (!roo::ProtoBuf::marshalling_to_string(request, &mar_str)) {
            std::cerr << "marshalling message failed." << std::endl;
            return -1;
        }

        std::string resp_str;
        auto status = client.call_RPC(tzrpc::ServiceID::CLIENT_SERVICE,
                                      rong::Client::OpCode::kSelect,
                                      mar_str, resp_str);
        if (status != RpcClientStatus::OK) {
            std::cerr << "call failed, return code [" << static_cast<int>(status) << "]" << std::endl;
            return -1;
        }

        rong::Client::StateMachineSelectOps::Response response;
        if (!roo::ProtoBuf::unmarshalling_from_string(resp_str, &response)) {
            std::cerr << "unmarshalling message failed." << std::endl;
            return -1;
        }

        if (!response.has_code() || response.code() != 0) {
            std::cerr << "response code check error" << std::endl;
            return -1;
        }

        std::cout << "read return ok with content:" << response.read().content() << std::endl;

    } else if (::strncmp(argv[1], "set", 3) == 0) {

        if (argc < 4)
            usage();

        std::string key = argv[2];
        std::string val = argv[3];

        std::string mar_str;
        rong::Client::StateMachineUpdateOps::Request request;
        request.mutable_write()->set_key(key);
        request.mutable_write()->set_content(val);
        if (!roo::ProtoBuf::marshalling_to_string(request, &mar_str)) {
            std::cerr << "marshalling message failed." << std::endl;
            return -1;
        }

        std::string resp_str;
        auto status = client.call_RPC(tzrpc::ServiceID::CLIENT_SERVICE,
                                      rong::Client::OpCode::kUpdate,
                                      mar_str, resp_str);
        if (status != RpcClientStatus::OK) {
            std::cerr << "call failed, return code [" << static_cast<int>(status) << "]" << std::endl;
            return -1;
        }

        rong::Client::StateMachineUpdateOps::Response response;
        if (!roo::ProtoBuf::unmarshalling_from_string(resp_str, &response)) {
            std::cerr << "unmarshalling message failed." << std::endl;
            return -1;
        }

        if (!response.has_code() || response.code() != 0) {
            std::cerr << "response code check error" << std::endl;
            return -1;
        }

        std::cout << "write return ok with context:" << response.context() << std::endl;

    } else if (::strncmp(argv[1], "del", 3) == 0) {

        if (argc < 3)
            usage();

        std::string key = argv[2];

        std::string mar_str;
        rong::Client::StateMachineUpdateOps::Request request;
        request.mutable_remove()->set_key(key);
        if (!roo::ProtoBuf::marshalling_to_string(request, &mar_str)) {
            std::cerr << "marshalling message failed." << std::endl;
            return -1;
        }

        std::string resp_str;
        auto status = client.call_RPC(tzrpc::ServiceID::CLIENT_SERVICE,
                                      rong::Client::OpCode::kUpdate,
                                      mar_str, resp_str);
        if (status != RpcClientStatus::OK) {
            std::cerr << "call failed, return code [" << static_cast<int>(status) << "]" << std::endl;
            return -1;
        }

        rong::Client::StateMachineUpdateOps::Response response;
        if (!roo::ProtoBuf::unmarshalling_from_string(resp_str, &response)) {
            std::cerr << "unmarshalling message failed." << std::endl;
            return -1;
        }

        if (!response.has_code() || response.code() != 0) {
            std::cerr << "response code check error" << std::endl;
            return -1;
        }

        std::cout << "delete return ok with context: " << response.context() << std::endl;

    } else if (::strncmp(argv[1], "keys", 4) == 0) {

        std::string mar_str;
        rong::Client::StateMachineSelectOps::Request request;
        request.mutable_range()->set_limit(0);
        if (!roo::ProtoBuf::marshalling_to_string(request, &mar_str)) {
            std::cerr << "marshalling message failed." << std::endl;
            return -1;
        }

        std::string resp_str;
        auto status = client.call_RPC(tzrpc::ServiceID::CLIENT_SERVICE,
                                      rong::Client::OpCode::kSelect,
                                      mar_str, resp_str);
        if (status != RpcClientStatus::OK) {
            std::cerr << "call failed, return code [" << static_cast<int>(status) << "]" << std::endl;
            return -1;
        }

        rong::Client::StateMachineSelectOps::Response response;
        if (!roo::ProtoBuf::unmarshalling_from_string(resp_str, &response)) {
            std::cerr << "unmarshalling message failed." << std::endl;
            return -1;
        }

        if (!response.has_code() || response.code() != 0) {
            std::cerr << "response code check error" << std::endl;
            return -1;
        }

        int size = response.range().contents_size();
        std::cout << "GOOD, return: " << size << std::endl;
        for (int i = 0; i < size; ++i) {
            auto k = response.range().contents(i);
            std::cout << " : " << k << std::endl;
        }
    } else {
        usage();
    }


    return 0;
}
