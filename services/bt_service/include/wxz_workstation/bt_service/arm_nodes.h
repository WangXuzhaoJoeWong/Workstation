#pragma once

#include <cstdint>
#include <string>

#include <behaviortree_cpp_v3/bt_factory.h>

namespace wxz::core {
class FastddsChannel;
}

namespace wxz::workstation::bt_service {

struct ArmRespCache;
struct TraceContext;

struct ArmNodeDeps {
    wxz::core::FastddsChannel* arm_cmd_dto_pub{nullptr};
    std::string arm_cmd_dto_topic;
    std::string arm_cmd_dto_schema;

    wxz::core::FastddsChannel* system_alert_dto_pub{nullptr};
    std::string system_alert_dto_topic;
    std::string system_alert_dto_schema;

    std::string dto_source;

    ArmRespCache* arm_cache{nullptr};
    std::uint64_t arm_timeout_ms{30'000};
    TraceContext* trace_ctx{nullptr};
};

void register_arm_control_nodes(BT::BehaviorTreeFactory& factory, const ArmNodeDeps& deps);

}  // namespace wxz::workstation::bt_service
