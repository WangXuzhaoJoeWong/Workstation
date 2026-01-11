#pragma once

#include <string>

#include "wxz_workstation/arm_control/internal/arm_control_internal.h"

namespace wxz::workstation::arm_control::internal {

struct ArmCommand {
    std::string raw;
    EventDTOUtil::KvMap kv;
    std::string op;
    std::string id;
};

ArmCommand parse_arm_command(const std::string& raw);

// Command handler function signature.
using ArmCommandHandler = EventDTOUtil::KvMap (*)(const ArmCommand& cmd, IArmClient& arm, const Logger& logger);

// Register a handler for a given operation key (e.g., "moveL").
void register_arm_handler(const std::string& op, ArmCommandHandler fn);

// Initialize the default handlers for the built-in operations.
void init_default_arm_handlers();

// Returns a KV response payload to publish on /arm/status (DTO envelope).
EventDTOUtil::KvMap handle_arm_command(const ArmCommand& cmd, IArmClient& arm, const Logger& logger);

} // namespace wxz::workstation::arm_control::internal
