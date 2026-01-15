#pragma once

#include <string>

#include "logger.h"

namespace wxz::core {
struct NodeBaseConfig;
}

namespace wxz::workstation::bt_service {

struct AppConfig;

/// 构造 bt_service 的 NodeBase 配置。
///
/// 该配置通常包含 domain、节点名/能力 topic、健康探针文件等。
wxz::core::NodeBaseConfig make_bt_node_config(const AppConfig& cfg, const wxz::core::Logger& logger);

}  // namespace wxz::workstation::bt_service
