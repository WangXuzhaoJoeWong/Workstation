#include "main_loop.h"

#include <chrono>

#include "executor.h"
#include "node_base.h"
#include "bt_tree_runner.h"

namespace wxz::workstation::bt_service {

void run_bt_main_loop(wxz::core::NodeBase& node,
                      wxz::core::Executor& exec,
                      BtTreeRunner& tree_runner,
                      int tick_ms) {
    const auto tick_dur = std::chrono::milliseconds(tick_ms);
    while (node.running()) {
        const auto loop_start = std::chrono::steady_clock::now();

        node.tick();
        tree_runner.maybe_reload();
        tree_runner.tick_once();

        for (;;) {
            if (!node.running()) return;
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - loop_start);
            if (elapsed >= tick_dur) break;

            const auto remaining = tick_dur - elapsed;
            const auto slice = (remaining > std::chrono::milliseconds(5)) ? std::chrono::milliseconds(5) : remaining;
            (void)exec.spin_once(slice);
        }
    }
}

}  // namespace wxz::workstation::bt_service
