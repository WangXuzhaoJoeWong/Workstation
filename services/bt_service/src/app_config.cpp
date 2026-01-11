#include "wxz_workstation/bt_service/app_config.h"

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
    cfg.bt.xml_path = wxz::core::getenv_str("WXZ_BT_XML", "bt.xml");
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

    return cfg;
}

}  // namespace wxz::workstation::bt_service
