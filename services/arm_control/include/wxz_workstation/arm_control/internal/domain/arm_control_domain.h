#pragma once

#include <string>

#include "dto/event_dto.h"

#include "logger.h"

namespace wxz::workstation::arm_control::internal {
class IArmClient;
}

namespace wxz::workstation::arm_control::domain {

class ArmControlDomain {
public:
    // Domain entry: takes a raw KV payload (from EventDTO.payload) and returns a KV response.
    // No DDS concerns here.
    EventDTOUtil::KvMap handle_raw_command(const std::string& raw,
                                          internal::IArmClient& arm,
                                          const wxz::core::Logger& logger) const;
};

} // namespace wxz::workstation::arm_control::domain
