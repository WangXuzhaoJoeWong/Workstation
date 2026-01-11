#include "wxz_workstation/bt_service/app.h"

#include <iostream>
#include <string>

#include <behaviortree_cpp_v3/bt_factory.h>

#include "node_base.h"
#include "service_common.h"
#include "logger.h"

#include "wxz_workstation/bt_service/app_config.h"
#include "wxz_workstation/bt_service/arm_wiring.h"
#include "wxz_workstation/bt_service/arm_types.h"
#include "wxz_workstation/bt_service/bt_runtime_wiring.h"
#include "wxz_workstation/bt_service/bt_tree_runner.h"
#include "wxz_workstation/bt_service/dds_channels.h"
#include "wxz_workstation/bt_service/main_loop.h"
#include "wxz_workstation/bt_service/node_wiring.h"

static int bt_service_main_impl() {
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);

    const auto cfg = wxz::workstation::bt_service::load_app_config_from_env();

    const auto log_level = wxz::core::parse_log_level(wxz::core::getenv_str("WXZ_LOG_LEVEL", "info"));
    auto& logger = wxz::core::Logger::getInstance();
    logger.set_level(log_level);
    logger.set_prefix("[workstation_bt_service] ");

    wxz::core::NodeBase node(wxz::workstation::bt_service::make_bt_node_config(cfg, logger));
    node.install_signal_handlers();

    logger.log(wxz::core::LogLevel::Info,
               "start domain=" + std::to_string(cfg.domain) + " xml='" + cfg.bt.xml_path + "' tick_ms=" +
                   std::to_string(cfg.bt.tick_ms) + " reload_ms=" + std::to_string(cfg.bt.reload_ms));

    wxz::workstation::bt_service::ArmRespCache arm_cache;
    wxz::workstation::bt_service::TraceContext trace_ctx;

    auto channels = wxz::workstation::bt_service::make_dds_channels(cfg);

    BT::BehaviorTreeFactory factory;
    wxz::workstation::bt_service::setup_arm_control_bt(factory, cfg, channels, arm_cache, trace_ctx);

    auto tree_runner = wxz::workstation::bt_service::make_bt_tree_runner(factory, cfg.bt, logger);

    wxz::workstation::bt_service::run_bt_main_loop(node, *tree_runner, cfg.bt.tick_ms);

    logger.log(wxz::core::LogLevel::Info, "stop");
    return 0;
}

namespace wxz::workstation::bt_service {

int run() {
    return bt_service_main_impl();
}

}  // namespace wxz::workstation::bt_service
