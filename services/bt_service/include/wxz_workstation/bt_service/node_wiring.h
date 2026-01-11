#pragma once

#include <string>

#include "logger.h"

namespace wxz::core {
struct NodeBaseConfig;
}

namespace wxz::workstation::bt_service {

struct AppConfig;

wxz::core::NodeBaseConfig make_bt_node_config(const AppConfig& cfg, const wxz::core::Logger& logger);

}  // namespace wxz::workstation::bt_service
