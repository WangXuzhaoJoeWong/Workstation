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

/// arm_control 相关 BT 节点的依赖集合（topic/schema、发布通道、缓存等）。
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

/// 向工厂注册 arm_control 相关 BT 节点。
///
/// deps 通过值传递（内部复制/保存必要字段）。调用方需保证 deps 指针字段在运行期有效。
void register_arm_control_nodes(BT::BehaviorTreeFactory& factory, ArmNodeDeps deps);

}  // namespace wxz::workstation::bt_service
