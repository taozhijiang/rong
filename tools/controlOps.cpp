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
#include <Protocol/gen-cpp/Control.pb.h>

static void usage() {
    std::stringstream ss;

    ss << std::endl
        << "./controlOps snapshot port [ip]" << std::endl
        << "             stat     port [ip] " << std::endl
        << std::endl;

    std::cout << ss.str();

    ::exit(EXIT_SUCCESS);
}

using namespace tzrpc_client;

int main(int argc, char* argv[]) {

    if (argc < 3)
        usage();
    
    std::string srv_addr = "127.0.0.1";
    int         srv_port = ::atoi(argv[2]);
    if (argc >= 4)
        srv_addr = std::string(argv[3]);
    RpcClient client(srv_addr, srv_port);
    
    
    if (::strncmp(argv[1], "snapshot", 8) == 0) {

        std::string mar_str;
        rong::Control::ControlSnapshotOps::Request request;
        request.mutable_snapshot()->set_hint("sp");
        if (!roo::ProtoBuf::marshalling_to_string(request, &mar_str)) {
            std::cerr << "marshalling message failed." << std::endl;
            return -1;
        }

        std::string resp_str;
        auto status = client.call_RPC(tzrpc::ServiceID::CONTROL_SERVICE,
                                      rong::Control::OpCode::kSnapshot,
                                      mar_str, resp_str);
        if (status != RpcClientStatus::OK) {
            std::cerr << "call failed, return code [" << static_cast<uint8_t>(status) << "]" << std::endl;
            return -1;
        }

        rong::Control::ControlSnapshotOps::Response response;
        if (!roo::ProtoBuf::unmarshalling_from_string(resp_str, &response)) {
            std::cerr << "unmarshalling message failed." << std::endl;
            return -1;
        }

        if (!response.has_code() || response.code() != 0) {
            std::cerr << "response code check error" << std::endl;
            return -1;
        }

        std::cout << "GOOD, return hint: " << response.snapshot().hint() << std::endl;

    } else if (::strncmp(argv[1], "stat", 4) == 0) {

        std::string mar_str;
        rong::Control::ControlStatOps::Request request;
        request.mutable_stat()->set_hint("st");
        if (!roo::ProtoBuf::marshalling_to_string(request, &mar_str)) {
            std::cerr << "marshalling message failed." << std::endl;
            return -1;
        }

        std::string resp_str;
        auto status = client.call_RPC(tzrpc::ServiceID::CONTROL_SERVICE,
                                      rong::Control::OpCode::kStat,
                                      mar_str, resp_str);
        if (status != RpcClientStatus::OK) {
            std::cerr << "call failed, return code [" << static_cast<uint8_t>(status) << "]" << std::endl;
            return -1;
        }

        rong::Control::ControlStatOps::Response response;
        if (!roo::ProtoBuf::unmarshalling_from_string(resp_str, &response)) {
            std::cerr << "unmarshalling message failed." << std::endl;
            return -1;
        }

        if (!response.has_code() || response.code() != 0) {
            std::cerr << "response code check error" << std::endl;
            return -1;
        }

        std::cout << "stat return ok with hint: " << response.stat().context() << std::endl;

    } else {
        usage();
    }


    return 0;
}
