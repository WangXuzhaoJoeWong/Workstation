#include "dds_channels.h"

#include <memory>

#include "fastdds_channel.h"
#include "service_common.h"
#include "app_config.h"

namespace wxz::workstation::bt_service {

DdsChannels make_dds_channels(const AppConfig& cfg) {
    DdsChannels ch;

    ch.arm_cmd_dto_pub = std::make_unique<wxz::core::FastddsChannel>(
        cfg.domain,
        cfg.arm.cmd_dto_topic,
        wxz::core::default_reliable_qos(),
        cfg.dto.max_payload,
        /*启用发布=*/true,
        /*启用订阅=*/false);

    ch.arm_status_dto_sub = std::make_unique<wxz::core::FastddsChannel>(
        cfg.domain,
        cfg.arm.status_dto_topic,
        wxz::core::default_reliable_qos(),
        cfg.dto.max_payload,
        /*启用发布=*/false,
        /*启用订阅=*/true);

    ch.system_alert_dto_pub = std::make_unique<wxz::core::FastddsChannel>(
        cfg.domain,
        cfg.system_alert.dto_topic,
        wxz::core::default_reliable_qos(),
        cfg.dto.max_payload,
        /*启用发布=*/true,
        /*启用订阅=*/false);

    return ch;
}

}  // namespace wxz::workstation::bt_service
