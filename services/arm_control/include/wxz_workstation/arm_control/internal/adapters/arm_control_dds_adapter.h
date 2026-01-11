#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

#include "logger.h"

namespace wxz::core {
class NodeBase;
}

namespace wxz::workstation::arm_control::internal {
class CmdQueue;
class IArmClient;
struct StatusPublisher;
}

namespace wxz::workstation::arm_control::domain {
class ArmControlDomain;
}

namespace wxz::workstation::arm_control::adapters {

struct ArmControlTopics {
    int domain{0};

    // DTO-only topics (FastDDS payload is EventDTO CDR bytes)
    std::string cmd_dto_topic;
    std::string cmd_dto_schema;
    std::string status_dto_topic;
    std::string status_dto_schema;

    std::string fault_action_topic{"fault/action"};
    std::size_t dto_max_payload{8 * 1024};
    std::string dto_source{"workstation_arm_control_service"};
};

class ArmControlDdsAdapter {
public:
    explicit ArmControlDdsAdapter(ArmControlTopics topics);

    // Wiring: subscribe cmd(dto) -> queue; loop pop -> domain -> publish status(dto).
    // Returns false if the node is requested to stop.
    bool run(wxz::core::NodeBase& node,
             internal::CmdQueue& queue,
             internal::StatusPublisher& status_pub,
             const domain::ArmControlDomain& domain,
             internal::IArmClient& arm,
             const wxz::core::Logger& logger,
             std::chrono::milliseconds pop_timeout = std::chrono::milliseconds(200));

private:
    ArmControlTopics topics_;
};

} // namespace wxz::workstation::arm_control::adapters
