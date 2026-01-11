#include "wxz_workstation/bt_service/main_loop.h"

#include <chrono>

#include "node_base.h"
#include "wxz_workstation/bt_service/bt_tree_runner.h"

namespace wxz::workstation::bt_service {

void run_bt_main_loop(wxz::core::NodeBase& node, BtTreeRunner& tree_runner, int tick_ms) {
    while (node.running()) {
        node.tick();
        tree_runner.maybe_reload();
        tree_runner.tick_once();
        if (!node.sleep_for(std::chrono::milliseconds(tick_ms))) break;
    }
}

}  // namespace wxz::workstation::bt_service
