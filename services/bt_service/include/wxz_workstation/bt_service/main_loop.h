#pragma once

namespace wxz::core {
class NodeBase;
}

namespace wxz::workstation::bt_service {

class BtTreeRunner;

void run_bt_main_loop(wxz::core::NodeBase& node, BtTreeRunner& tree_runner, int tick_ms);

}  // namespace wxz::workstation::bt_service
