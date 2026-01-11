#pragma once

#include <string>

#include "dto/event_dto.h"

namespace wxz::workstation::arm_control::internal {
class IArmClient;
struct Logger;
}

namespace wxz::workstation::arm_control::domain {

class ArmControlDomain {
public:
    // Domain entry: takes a raw KV payload and returns a KV response.
    // No DDS concerns here.
    EventDTOUtil::KvMap handle_raw_command(const std::string& raw,
                                          internal::IArmClient& arm,
                                          const internal::Logger& logger) const;
};

} // namespace wxz::workstation::arm_control::domain
