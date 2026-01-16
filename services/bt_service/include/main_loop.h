#pragma once

#include "workstation/node.h"

namespace wxz::workstation::bt_service {

class BtTreeRunner;

/// bt_service 主循环。
///
/// 在循环中按 tick_ms 频率 tick 行为树，并由 node.executor() 统一驱动异步任务（spin_once）。
/// 该函数通常在 run() 内被调用，直到节点退出/收到停止信号。
void run_bt_main_loop(wxz::workstation::Node& node,
					  BtTreeRunner& tree_runner,
					  int tick_ms);

}  // namespace wxz::workstation::bt_service
