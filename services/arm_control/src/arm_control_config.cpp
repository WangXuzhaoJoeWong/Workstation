#include "internal/arm_control_config.h"

#include "logger.h"

namespace wxz::workstation::arm_control::internal {

ArmControlConfig load_arm_control_config_from_env() {
    ArmControlConfig cfg;

    cfg.conn.ip = Env::get_str("WXZ_ARM_IP", "192.168.100.88");
    cfg.conn.port = Env::get_int("WXZ_ARM_PORT", 2323);
    cfg.conn.passwd = Env::get_str("WXZ_ARM_PASS", "123");

    cfg.domain = Env::get_int("WXZ_DOMAIN_ID", 0);

    cfg.cmd_dto_topic = Env::get_str("WXZ_P1_ARM_COMMAND_TOPIC", "/arm/command");
    cfg.cmd_dto_schema = Env::get_str("WXZ_ARM_CMD_DTO_SCHEMA", "ws.arm_command.v1");
    cfg.status_dto_topic = Env::get_str("WXZ_P1_ARM_STATUS_TOPIC", "/arm/status");
    cfg.status_dto_schema = Env::get_str("WXZ_ARM_STATUS_DTO_SCHEMA", "ws.arm_status.v1");
    cfg.dto_source = Env::get_str("WXZ_DTO_SOURCE", "workstation_arm_control_service");
    cfg.dto_max_payload = Env::get_size("WXZ_DTO_MAX_PAYLOAD", 8192);

    cfg.capability_topic = Env::get_str("WXZ_CAPABILITY_STATUS_TOPIC", "capability/status");
    cfg.fault_status_topic = Env::get_str("WXZ_FAULT_STATUS_TOPIC", "fault/status");
    cfg.fault_action_topic = Env::get_str("WXZ_FAULT_ACTION_TOPIC", "fault/action");
    cfg.heartbeat_topic = Env::get_str("WXZ_HEARTBEAT_STATUS_TOPIC", "heartbeat/status");
    cfg.heartbeat_period_ms = Env::get_int("WXZ_HEARTBEAT_PERIOD_MS", 1000);
    cfg.timesync_period_ms = Env::get_int("WXZ_TIMESYNC_PERIOD_MS", 5000);
    cfg.timesync_scope = Env::get_str("WXZ_TIMESYNC_SCOPE", "");
    cfg.health_file = Env::get_str("WXZ_HEALTH_FILE", "");

    cfg.queue_max = Env::get_size("WXZ_ARM_QUEUE_MAX", 64);
    cfg.sw_version = Env::get_str("WXZ_SW_VERSION", "dev");

    cfg.rpc_enable = Env::get_int("WXZ_ARM_RPC_ENABLE", 0);
    cfg.rpc_req_topic = Env::get_str("WXZ_ARM_RPC_REQUEST_TOPIC", "/svc/arm_control/rpc/request");
    cfg.rpc_rep_topic = Env::get_str("WXZ_ARM_RPC_REPLY_TOPIC", "/svc/arm_control/rpc/reply");
    cfg.rpc_service_name = Env::get_str("WXZ_ARM_RPC_SERVICE_NAME", "workstation_arm_control_service");

    cfg.log_level = wxz::core::parse_log_level(Env::get_str("WXZ_LOG_LEVEL", "info"));

    // Keep current behavior: hard-coded metrics scope.
    cfg.metrics_scope = "workstation_arm_control_service";

    return cfg;
}

} // namespace wxz::workstation::arm_control::internal
