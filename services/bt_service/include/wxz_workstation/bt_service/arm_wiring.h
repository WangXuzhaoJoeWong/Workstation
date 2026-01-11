#pragma once

namespace BT {
class BehaviorTreeFactory;
}

namespace wxz::workstation::bt_service {

struct AppConfig;
struct DdsChannels;
class ArmRespCache;
struct TraceContext;

void setup_arm_control_bt(BT::BehaviorTreeFactory& factory,
                          const AppConfig& cfg,
                          DdsChannels& channels,
                          ArmRespCache& arm_cache,
                          TraceContext& trace_ctx);

}  // namespace wxz::workstation::bt_service
