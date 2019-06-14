/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PAXOS_PEER_H__
#define __PAXOS_PEER_H__

#include <xtra_rhel.h>

#include <Client/include/RpcClient.h>


// 集群中的每一个成员(除了本身)会使用Peer来管理

namespace rong {

using tzrpc_client::rpc_handler_t;
using tzrpc_client::RpcClient;


class Peer {

public:
    Peer(uint64_t id,
         const std::string& addr, uint16_t port, const rpc_handler_t& handler);
    ~Peer() = default;

    int send_RPC_async(uint16_t service_id, uint16_t opcode, const std::string& payload) const;

    int send_RPC_sync(uint16_t service_id, uint16_t opcode,
                      const std::string& payload, std::string& respload) const;

    std::string str() const {

        std::stringstream ss;
        ss  << "Paxos Peer Info: " << std::endl
            << "    id: " << id_ << std::endl
            << "    addr port: " << addr_ << " " << port_ << std::endl;

        return ss.str();
    }

public:

    const uint64_t id_;

    // Endpoint信息
    const std::string addr_;
    const uint16_t port_;

private:

    rpc_handler_t handler_;

    // Paxos(包括Basic,Lease)协议使用的RPC
    std::unique_ptr<RpcClient> rpc_client_;

    // 客户端请求的RPC转发(到Leader)
    std::unique_ptr<RpcClient> rpc_proxy_;

    friend std::ostream& operator<<(std::ostream& os, const Peer& peer);

};

} // namespace rong

#endif // __PAXOS_PEER_H__
