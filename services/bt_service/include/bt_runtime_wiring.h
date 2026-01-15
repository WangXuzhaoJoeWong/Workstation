#pragma once

#include <memory>

#include "logger.h"

namespace BT {
class BehaviorTreeFactory;
}

namespace wxz::workstation::bt_service {

struct BtConfig;
class BtTreeRunner;

/// 创建并配置 BtTreeRunner（包含 XML 路径、热加载周期、logger 等）。
std::unique_ptr<BtTreeRunner> make_bt_tree_runner(BT::BehaviorTreeFactory& factory,
                                                  const BtConfig& cfg,
                                                  const wxz::core::Logger& logger);

}  // namespace wxz::workstation::bt_service
