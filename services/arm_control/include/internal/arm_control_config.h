#pragma once

#include <cstddef>
#include <string>

#include "internal/arm_control_internal.h"

namespace wxz::workstation::arm_control::internal {

struct ArmControlConfig {
    ArmConn conn;

    int domain{0};

    // 仅 DTO（FastDDS 负载为 EventDTO 的 CDR 字节流）
    std::string cmd_dto_topic{"/arm/command"};
    std::string cmd_dto_schema{"ws.arm_command.v1"};
    std::string status_dto_topic{"/arm/status"};
    std::string status_dto_schema{"ws.arm_status.v1"};
    std::string dto_source{"workstation_arm_control_service"};
    std::size_t dto_max_payload{8192};

    // NodeBase / health/fault
    std::string capability_topic{"capability/status"};
    std::string fault_status_topic{"fault/status"};
    std::string fault_action_topic{"fault/action"};
    std::string heartbeat_topic{"heartbeat/status"};
    int heartbeat_period_ms{1000};
    int timesync_period_ms{5000};
    std::string timesync_scope;
    std::string health_file;

    std::size_t queue_max{64};
    std::string sw_version{"dev"};

    // RPC control plane
    int rpc_enable{0};
    std::string rpc_req_topic{"/svc/arm_control/rpc/request"};
    std::string rpc_rep_topic{"/svc/arm_control/rpc/reply"};
    std::string rpc_service_name{"workstation_arm_control_service"};

    // Observability
    LogLevel log_level{LogLevel::Info};

    // Metrics scope used by subscriptions
    std::string metrics_scope{"workstation_arm_control_service"};
};

ArmControlConfig load_arm_control_config_from_env();

} // namespace wxz::workstation::arm_control::internal
