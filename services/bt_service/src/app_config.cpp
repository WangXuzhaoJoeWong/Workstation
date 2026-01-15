#include "app_config.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "service_common.h"

namespace wxz::workstation::bt_service {

AppConfig load_app_config_from_env() {
    AppConfig cfg;

    cfg.domain = wxz::core::getenv_int("WXZ_DOMAIN_ID", 0);

    cfg.arm.cmd_dto_topic = wxz::core::getenv_str("WXZ_P1_ARM_COMMAND_TOPIC", "/arm/command");
    cfg.arm.cmd_dto_schema = wxz::core::getenv_str("WXZ_ARM_CMD_DTO_SCHEMA", "ws.arm_command.v1");
    cfg.arm.status_dto_topic = wxz::core::getenv_str("WXZ_P1_ARM_STATUS_TOPIC", "/arm/status");
    cfg.arm.timeout_ms = static_cast<std::uint64_t>(wxz::core::getenv_int("WXZ_ARM_CMD_TIMEOUT_MS", 30000));

    cfg.dto.source = wxz::core::getenv_str("WXZ_DTO_SOURCE", "workstation_bt_service");
    cfg.dto.max_payload = static_cast<std::size_t>(wxz::core::getenv_int("WXZ_DTO_MAX_PAYLOAD", 8192));

    cfg.capability_topic = wxz::core::getenv_str("WXZ_CAPABILITY_STATUS_TOPIC", "capability/status");
    cfg.fault_status_topic = wxz::core::getenv_str("WXZ_FAULT_STATUS_TOPIC", "fault/status");
    cfg.heartbeat_topic = wxz::core::getenv_str("WXZ_HEARTBEAT_STATUS_TOPIC", "heartbeat/status");
    cfg.heartbeat_period_ms = wxz::core::getenv_int("WXZ_HEARTBEAT_PERIOD_MS", 1000);
    cfg.timesync_period_ms = wxz::core::getenv_int("WXZ_TIMESYNC_PERIOD_MS", 5000);
    cfg.timesync_scope = wxz::core::getenv_str("WXZ_TIMESYNC_SCOPE", "");

    // 始终使用相对当前工作目录的路径。
    // 注意：WXZ_BT_XML 被刻意忽略，以避免部署漂移（不同机器/环境指向不同 BT 文件）。
    cfg.bt.xml_path = "bt.xml";
    cfg.bt.tick_ms = wxz::core::getenv_int("WXZ_BT_TICK_MS", 20);
    cfg.bt.reload_ms = wxz::core::getenv_int("WXZ_BT_RELOAD_MS", 500);

    cfg.health_file = wxz::core::getenv_str("WXZ_HEALTH_FILE", "");
    cfg.sw_version = wxz::core::getenv_str("WXZ_SW_VERSION", "dev");

    cfg.system_alert.dto_topic = wxz::core::getenv_str("WXZ_SYSTEM_ALERT_TOPIC", "/system/alert");
    cfg.system_alert.dto_schema = wxz::core::getenv_str("WXZ_SYSTEM_ALERT_DTO_SCHEMA", "ws.system_alert.v1");

    cfg.bt.groot.enable = wxz::core::getenv_int("WXZ_BT_GROOT", 1);
    cfg.bt.groot.port = wxz::core::getenv_int("WXZ_BT_GROOT_PORT", 1666);
    cfg.bt.groot.server_port = wxz::core::getenv_int("WXZ_BT_GROOT_SERVER_PORT", -1);
    cfg.bt.groot.retry = wxz::core::getenv_int("WXZ_BT_GROOT_RETRY", 5);
    cfg.bt.groot.max_msg_per_sec = wxz::core::getenv_int("WXZ_BT_GROOT_MAX_MSG_PER_SEC", 25);

    // 最小化的 RPC 控制面（基于 FastDDS topic）。
    // 默认禁用，避免部署时发生 topic 冲突/碰撞。
    cfg.rpc.enable = wxz::core::getenv_int("WXZ_BT_RPC_ENABLE", 0);
    cfg.rpc.request_topic = wxz::core::getenv_str("WXZ_BT_RPC_REQUEST_TOPIC", "/svc/bt_service/rpc/request");
    cfg.rpc.reply_topic = wxz::core::getenv_str("WXZ_BT_RPC_REPLY_TOPIC", "/svc/bt_service/rpc/reply");
    cfg.rpc.service_name = wxz::core::getenv_str("WXZ_BT_RPC_SERVICE_NAME", "workstation_bt_service");

    return cfg;
}

}  // namespace wxz::workstation::bt_service
