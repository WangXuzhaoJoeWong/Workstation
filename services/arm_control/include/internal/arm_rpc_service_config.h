#pragma once

#include <string>

#include "workstation/service.h"

namespace wxz::workstation::arm_control::internal {

wxz::workstation::RpcService::Config make_arm_rpc_service_config(int domain_id,
                                                                 std::string request_topic,
                                                                 std::string reply_topic,
                                                                 std::string service_name,
                                                                 std::string sw_version);

} // namespace wxz::workstation::arm_control::internal
