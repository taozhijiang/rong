/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __PROTOCOL_COMMON_H__
#define __PROTOCOL_COMMON_H__

namespace tzrpc {

namespace ServiceID {

enum {
    PAXOS_SERVICE   = 1,   //
    CLIENT_SERVICE  = 2,   // Client Select and StateMachine update
    CONTROL_SERVICE = 3,   // 控制服务，比如创建快照、打印集群信息等
};

}

} // end namespace tzrpc


#endif // __PROTOCOL_COMMON_H__
