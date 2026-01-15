#pragma once

#include "logger.h"
#include "workstation/service.h"

namespace wxz::workstation::arm_control::internal {

class ArmCommandProcessor;
class IArmClient;

void install_arm_rpc_handlers(wxz::workstation::RpcService& rpc_server,
                              ArmCommandProcessor& processor,
                              IArmClient& arm,
                              wxz::core::Logger& logger);

} // namespace wxz::workstation::arm_control::internal
