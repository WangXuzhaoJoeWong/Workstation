#include "wxz_workstation/bt_service/node_wiring.h"

#include <string>

#include "node_base.h"
#include "logger.h"
#include "wxz_workstation/bt_service/app_config.h"

namespace wxz::workstation::bt_service {

wxz::core::NodeBaseConfig make_bt_node_config(const AppConfig& cfg, const wxz::core::Logger& logger) {
    wxz::core::NodeBaseConfig nc;
    nc.service = "workstation_bt_service";
    nc.type = "bt";

    nc.version = cfg.sw_version;
    nc.api_version = 1;
    nc.schema_version = 1;
    nc.domain = cfg.domain;

    nc.health_file = cfg.health_file;
    nc.capability_topic = cfg.capability_topic;
    nc.health_period_ms = 1000;
    nc.capability_period_ms = 1000;

    nc.topics_pub = {cfg.arm.cmd_dto_topic, cfg.system_alert.dto_topic};
    nc.topics_sub = {cfg.arm.status_dto_topic};

    nc.warn = [&](const std::string& m) { logger.log(wxz::core::LogLevel::Warn, m); };

    return nc;
}

}  // namespace wxz::workstation::bt_service
