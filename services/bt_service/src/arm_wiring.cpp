#include "wxz_workstation/bt_service/arm_wiring.h"

#include <behaviortree_cpp_v3/bt_factory.h>

#include "wxz_workstation/bt_service/app_config.h"
#include "wxz_workstation/bt_service/arm_nodes.h"
#include "wxz_workstation/bt_service/arm_status_cache.h"
#include "wxz_workstation/bt_service/arm_types.h"
#include "wxz_workstation/bt_service/dds_channels.h"

namespace wxz::workstation::bt_service {

void setup_arm_control_bt(BT::BehaviorTreeFactory& factory,
                          const AppConfig& cfg,
                          DdsChannels& channels,
                          ArmRespCache& arm_cache,
                          TraceContext& trace_ctx) {
    install_arm_status_cache_updater(*channels.arm_status_dto_sub, arm_cache);

    register_arm_control_nodes(
        factory,
        ArmNodeDeps{
            .arm_cmd_dto_pub = channels.arm_cmd_dto_pub.get(),
            .arm_cmd_dto_topic = cfg.arm.cmd_dto_topic,
            .arm_cmd_dto_schema = cfg.arm.cmd_dto_schema,
            .system_alert_dto_pub = channels.system_alert_dto_pub.get(),
            .system_alert_dto_topic = cfg.system_alert.dto_topic,
            .system_alert_dto_schema = cfg.system_alert.dto_schema,
            .dto_source = cfg.dto.source,
            .arm_cache = &arm_cache,
            .arm_timeout_ms = cfg.arm.timeout_ms,
            .trace_ctx = &trace_ctx,
        });
}

}  // namespace wxz::workstation::bt_service
