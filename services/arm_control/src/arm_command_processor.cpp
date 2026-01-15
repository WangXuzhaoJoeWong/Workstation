#include "internal/arm_command_processor.h"

#include "internal/arm_command_handler.h"

namespace wxz::workstation::arm_control::internal {

EventDTOUtil::KvMap ArmCommandProcessor::handle_raw_command(const std::string& raw,
                                                           IArmClient& arm,
                                                           const wxz::core::Logger& logger) const {
    ArmCommand cmd = parse_arm_command(raw);
    return handle_arm_command(cmd, arm, logger);
}

}  // namespace wxz::workstation::arm_control::internal
