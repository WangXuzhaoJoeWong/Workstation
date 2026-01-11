#pragma once

#include <cstddef>
#include <memory>

namespace wxz::core {
class FastddsChannel;
}

namespace wxz::workstation::bt_service {

struct AppConfig;

struct DdsChannels {
    std::unique_ptr<wxz::core::FastddsChannel> arm_cmd_dto_pub;
    std::unique_ptr<wxz::core::FastddsChannel> arm_status_dto_sub;
    std::unique_ptr<wxz::core::FastddsChannel> system_alert_dto_pub;
};

DdsChannels make_dds_channels(const AppConfig& cfg);

}  // namespace wxz::workstation::bt_service
