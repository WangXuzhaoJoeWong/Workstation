#include "wxz_workstation/arm_control/internal/domain/arm_control_domain.h"

#include "wxz_workstation/arm_control/internal/arm_command_handler.h"

namespace wxz::workstation::arm_control::domain {

EventDTOUtil::KvMap ArmControlDomain::handle_raw_command(const std::string& raw,
                                                        internal::IArmClient& arm,
                                                        const wxz::core::Logger& logger) const {
    internal::ArmCommand cmd = internal::parse_arm_command(raw);
    return internal::handle_arm_command(cmd, arm, logger);
}

} // namespace wxz::workstation::arm_control::domain
