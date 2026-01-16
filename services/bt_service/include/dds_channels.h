#pragma once

#include <memory>

#include "workstation/node.h"

namespace wxz::workstation::bt_service {

struct AppConfig;

/// bt_service 使用的 DDS 通道集合（发布/订阅）。
struct DdsChannels {
    std::unique_ptr<wxz::workstation::EventDtoPublisher> arm_cmd_dto_pub;
    std::unique_ptr<wxz::workstation::EventDtoPublisher> system_alert_dto_pub;
};

/// 根据配置创建 bt_service 所需的 DDS 通道。
///
/// 说明：通道对象的生命周期由返回值持有者管理。
DdsChannels make_dds_channels(const AppConfig& cfg, wxz::workstation::Node& node);

}  // namespace wxz::workstation::bt_service
