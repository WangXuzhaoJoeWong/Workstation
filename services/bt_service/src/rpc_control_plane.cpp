#include "rpc_control_plane.h"

#include <string>

#include "app_config.h"
#include "bt_tree_runner.h"

#include "logger.h"
#include "node_base.h"
#include "rpc/rpc_service.h"
#include "service_common.h"
#include "strand.h"

namespace wxz::workstation::bt_service {

std::unique_ptr<wxz::core::rpc::RpcServer> start_bt_rpc_control_plane(const AppConfig& cfg,
                                                                      wxz::core::NodeBase& node,
                                                                      BtTreeRunner& tree_runner,
                                                                      wxz::core::Strand& rpc_strand,
                                                                      wxz::core::Logger& logger) {
    if (!cfg.rpc.enable) return nullptr;

    wxz::core::rpc::RpcServerOptions rpc_opts;
    rpc_opts.domain = cfg.domain;
    rpc_opts.request_topic = cfg.rpc.request_topic;
    rpc_opts.reply_topic = cfg.rpc.reply_topic;
    rpc_opts.service_name = cfg.rpc.service_name;

    auto rpc_server = std::make_unique<wxz::core::rpc::RpcServer>(std::move(rpc_opts));
    rpc_server->bind_scheduler(rpc_strand);

    using Json = wxz::core::rpc::RpcServer::Json;

    auto reload_result_to_string = [](TreeReloadResult r) -> std::string {
        switch (r) {
        case TreeReloadResult::Ok: return "ok";
        case TreeReloadResult::Unchanged: return "unchanged";
        case TreeReloadResult::ReadError: return "read_error";
        case TreeReloadResult::ParseError: return "parse_error";
        default: return "unknown";
        }
    };

    rpc_server->add_handler("bt.ping", [&](const Json&) {
        wxz::core::rpc::RpcServer::Reply rep;
        rep.result = Json{{"service", cfg.rpc.service_name},
                          {"sw_version", cfg.sw_version},
                          {"domain", cfg.domain},
                          {"ts_ms", wxz::core::now_epoch_ms()}};
        return rep;
    });

    rpc_server->add_handler("bt.reload", [&](const Json&) {
        const auto r = tree_runner.reload_if_changed();
        if (r == TreeReloadResult::Ok) {
            tree_runner.configure_groot1(cfg.bt.groot);
        }
        wxz::core::rpc::RpcServer::Reply rep;
        rep.result = Json{{"result", reload_result_to_string(r)}};
        return rep;
    });

    rpc_server->add_handler("bt.stop", [&](const Json&) {
        node.request_stop();
        wxz::core::rpc::RpcServer::Reply rep;
        rep.result = Json{{"requested", true}};
        return rep;
    });

    if (!rpc_server->start()) {
        logger.log(wxz::core::LogLevel::Warn, "RPC enabled but failed to start (ignored)");
        return nullptr;
    }

    logger.log(wxz::core::LogLevel::Info,
               "RPC enabled request='" + cfg.rpc.request_topic + "' reply='" + cfg.rpc.reply_topic + "'");
    return rpc_server;
}

}  // namespace wxz::workstation::bt_service
