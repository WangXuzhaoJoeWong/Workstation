#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace wxz::core {
class FastddsChannel;
class NodeBase;
struct ChannelQoS;
}

namespace wxz::workstation::arm_control::internal {
class CmdQueue;
class IArmClient;
struct Logger;
struct StatusPublisher;
}

namespace wxz::workstation::arm_control::domain {
class ArmControlDomain;
}

namespace wxz::workstation::arm_control::adapters {

struct ArmControlTopics {
    int domain{0};
    std::string cmd_topic;
    std::string status_topic;
    std::string fault_action_topic{"fault/action"};
};

class ArmControlDdsAdapter {
public:
    explicit ArmControlDdsAdapter(ArmControlTopics topics);

    // Wiring: subscribe cmd -> queue; loop pop -> domain -> publish status.
    // Returns false if the node is requested to stop.
    bool run(wxz::core::NodeBase& node,
             internal::CmdQueue& queue,
             internal::StatusPublisher& status_pub,
             const domain::ArmControlDomain& domain,
             internal::IArmClient& arm,
             const internal::Logger& logger,
             std::chrono::milliseconds pop_timeout = std::chrono::milliseconds(200));

private:
    ArmControlTopics topics_;
};

} // namespace wxz::workstation::arm_control::adapters
