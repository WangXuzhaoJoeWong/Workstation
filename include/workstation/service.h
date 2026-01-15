#pragma once

// Workstation public facade：实际实现位于 MotionCore/framework。

#include "framework/service.h"
#include "framework/service_client.h"

namespace wxz::workstation {

// 兼容别名：实际实现位于 MotionCore 的 framework 层。
using Status = wxz::framework::Status;
using RpcService = wxz::framework::RpcService;
using RpcServiceClient = wxz::framework::RpcServiceClient;

inline std::string default_rpc_request_topic(std::string_view service) {
    return wxz::framework::default_rpc_request_topic(service);
}

inline std::string default_rpc_reply_topic(std::string_view service) {
    return wxz::framework::default_rpc_reply_topic(service);
}

inline RpcService::Config default_rpc_service_config(int domain,
                                                    std::string_view service,
                                                    std::string_view sw_version,
                                                    std::string request_topic = {},
                                                    std::string reply_topic = {}) {
    return wxz::framework::default_rpc_service_config(
        domain, service, sw_version, std::move(request_topic), std::move(reply_topic));
}

inline RpcServiceClient::Config default_rpc_client_config(int domain,
                                                         std::string_view service,
                                                         std::string client_id_prefix = {},
                                                         std::string request_topic = {},
                                                         std::string reply_topic = {}) {
    return wxz::framework::default_rpc_client_config(
        domain, service, std::move(client_id_prefix), std::move(request_topic), std::move(reply_topic));
}

} // namespace wxz::workstation
