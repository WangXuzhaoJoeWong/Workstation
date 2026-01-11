#include "wxz_workstation/bt_service/bt_runtime_wiring.h"

#include <memory>

#include <behaviortree_cpp_v3/bt_factory.h>

#include "wxz_workstation/bt_service/app_config.h"
#include "wxz_workstation/bt_service/bt_tree_runner.h"

namespace wxz::workstation::bt_service {

std::unique_ptr<BtTreeRunner> make_bt_tree_runner(BT::BehaviorTreeFactory& factory,
                                                  const BtConfig& cfg,
                                                  const wxz::core::Logger& logger) {
    auto runner = std::make_unique<BtTreeRunner>(factory, cfg.xml_path, cfg.reload_ms, logger);
    (void)runner->reload_if_changed();
    runner->configure_groot1(cfg.groot);
    return runner;
}

}  // namespace wxz::workstation::bt_service
