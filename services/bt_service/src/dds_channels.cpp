#include "dds_channels.h"

#include <memory>

#include "app_config.h"

namespace wxz::workstation::bt_service {

DdsChannels make_dds_channels(const AppConfig& cfg, wxz::workstation::Node& node) {
    DdsChannels ch;

    ch.arm_cmd_dto_pub = node.create_publisher_eventdto(cfg.arm.cmd_dto_topic, cfg.dto.max_payload);

    ch.system_alert_dto_pub = node.create_publisher_eventdto(cfg.system_alert.dto_topic, cfg.dto.max_payload);

    return ch;
}

}  // namespace wxz::workstation::bt_service
