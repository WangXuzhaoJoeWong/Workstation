#include "wxz_workstation/arm_control/app.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>

#include "wxz_workstation/arm_control/internal/adapters/arm_control_dds_adapter.h"
#include "wxz_workstation/arm_control/internal/arm_command_handler.h"
#include "wxz_workstation/arm_control/internal/arm_control_internal.h"
#include "wxz_workstation/arm_control/internal/arm_error_codes.h"
#include "wxz_workstation/arm_control/internal/domain/arm_control_domain.h"

#include "node_base.h"
#include "service_common.h"
#include "logger.h"

namespace wxz::workstation::arm_control {

int run() {
    using namespace wxz::workstation::arm_control::internal;

    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);

    ArmConn conn;
    conn.ip = Env::get_str("WXZ_ARM_IP", "192.168.100.88");
    conn.port = Env::get_int("WXZ_ARM_PORT", 2323);
    conn.passwd = Env::get_str("WXZ_ARM_PASS", "123");

    const int domain = Env::get_int("WXZ_DOMAIN_ID", 0);

    // DTO-only (FastDDS payload is EventDTO CDR bytes)
    const std::string cmd_dto_topic = Env::get_str("WXZ_P1_ARM_COMMAND_TOPIC", "/arm/command");
    const std::string cmd_dto_schema = Env::get_str("WXZ_ARM_CMD_DTO_SCHEMA", "ws.arm_command.v1");
    const std::string status_dto_topic = Env::get_str("WXZ_P1_ARM_STATUS_TOPIC", "/arm/status");
    const std::string status_dto_schema = Env::get_str("WXZ_ARM_STATUS_DTO_SCHEMA", "ws.arm_status.v1");
    const std::string dto_source = Env::get_str("WXZ_DTO_SOURCE", "workstation_arm_control_service");

    const std::size_t dto_max = Env::get_size("WXZ_DTO_MAX_PAYLOAD", 8192);

    const std::string capability_topic = Env::get_str("WXZ_CAPABILITY_STATUS_TOPIC", "capability/status");
    const std::string fault_status_topic = Env::get_str("WXZ_FAULT_STATUS_TOPIC", "fault/status");
    const std::string fault_action_topic = Env::get_str("WXZ_FAULT_ACTION_TOPIC", "fault/action");
    const std::size_t queue_max = Env::get_size("WXZ_ARM_QUEUE_MAX", 64);
    const std::string health_file = Env::get_str("WXZ_HEALTH_FILE", "");
    const std::string sw_version = Env::get_str("WXZ_SW_VERSION", "dev");

    const LogLevel log_level = wxz::core::parse_log_level(Env::get_str("WXZ_LOG_LEVEL", "info"));
    auto& logger = wxz::core::Logger::getInstance();
    logger.set_level(log_level);
    logger.set_prefix("[workstation_arm_control_service] ");

    wxz::core::NodeBase node(wxz::core::NodeBaseConfig{
        .service = "workstation_arm_control_service",
        .type = "device.arm",
        .version = sw_version,
        .api_version = 1,
        .schema_version = 1,
        .domain = domain,
        .health_file = health_file,
        .capability_topic = capability_topic,
        .fault_topic = fault_status_topic,
        .health_period_ms = 1000,
        .capability_period_ms = 1000,
        .topics_pub = {status_dto_topic, fault_status_topic},
        .topics_sub = {cmd_dto_topic, fault_action_topic},
        .warn = [&](const std::string& m) { logger.log(LogLevel::Warn, m); },
    });
    node.install_signal_handlers();

    logger.log(LogLevel::Info,
               "start ip=" + conn.ip + " port=" + std::to_string(conn.port) +
                   " domain=" + std::to_string(domain) +
                   " cmd='" + cmd_dto_topic + "' status='" + status_dto_topic + "'");

    CmdQueue queue(queue_max);

    SdkApi api;
    const bool sdk_loaded = load_sdk(api, logger);
    if (!sdk_loaded || !api.cr_create_robot) {
        logger.log(LogLevel::Error, "SDK not available; simulation disabled; exiting");
        return 2;
    }
    logger.log(LogLevel::Info, "SDK ready (direct-linked)");

    std::unique_ptr<IArmClient> arm = std::make_unique<ArmSdkClient>(conn, &api);

    // Domain (business): KV command payload -> KV status payload.
    wxz::workstation::arm_control::domain::ArmControlDomain domain_logic;

    // Adapter (DDS): subscribe cmd -> queue; pop -> domain -> publish status.
    wxz::workstation::arm_control::adapters::ArmControlDdsAdapter dds_adapter(
        wxz::workstation::arm_control::adapters::ArmControlTopics{
            .domain = domain,
            .cmd_dto_topic = cmd_dto_topic,
            .cmd_dto_schema = cmd_dto_schema,
            .status_dto_topic = status_dto_topic,
            .status_dto_schema = status_dto_schema,
            .fault_action_topic = fault_action_topic,
            .dto_max_payload = dto_max,
            .dto_source = dto_source,
        });

    StatusPublisher status_pub(domain, status_dto_topic, status_dto_schema, dto_max, dto_source);

    (void)dds_adapter.run(node, queue, status_pub, domain_logic, *arm, logger);

    logger.log(LogLevel::Info, "stop");
    return 0;
}

} // namespace wxz::workstation::arm_control
