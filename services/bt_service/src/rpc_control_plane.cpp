#include "rpc_control_plane.h"

#include <string>

#include "app_config.h"
#include "bt_tree_runner.h"

#include "logger.h"
#include "service_common.h"
#include "strand.h"
#include "workstation/node.h"

namespace wxz::workstation::bt_service {

std::unique_ptr<wxz::workstation::RpcService> start_bt_rpc_control_plane(const AppConfig& cfg,
                                                                      wxz::workstation::Node& node,
                                                                      BtTreeRunner& tree_runner,
                                                                      wxz::core::Strand& rpc_strand,
                                                                      wxz::core::Logger& logger) {
    if (!cfg.rpc.enable) return nullptr;

    auto opts_builder = wxz::workstation::RpcService::Options::builder(cfg.rpc.service_name);
    opts_builder.sw_version(cfg.sw_version)
        .request_topic(cfg.rpc.request_topic)
        .reply_topic(cfg.rpc.reply_topic)
        .metrics_scope(cfg.rpc.service_name);

    auto rpc_server = node.create_service_on(
        rpc_strand,
        std::move(opts_builder).build());

    using Json = wxz::workstation::RpcService::Json;

    auto reload_result_to_string = [](TreeReloadResult r) -> std::string {
        switch (r) {
        case TreeReloadResult::Ok: return "ok";
        case TreeReloadResult::Unchanged: return "unchanged";
        case TreeReloadResult::ReadError: return "read_error";
        case TreeReloadResult::ParseError: return "parse_error";
        default: return "unknown";
        }
    };

    rpc_server->add_ping_handler("bt.ping");

    rpc_server->add_handler("bt.reload", [&](const Json&) {
        const auto r = tree_runner.reload_if_changed();
        if (r == TreeReloadResult::Ok) {
            tree_runner.configure_groot1(cfg.bt.groot);
        }
        wxz::workstation::RpcService::Reply rep;
        rep.status = wxz::workstation::Status::ok_status();
        rep.result = Json{{"result", reload_result_to_string(r)}};
        return rep;
    });

    rpc_server->add_handler("bt.stop", [&](const Json&) {
        node.base().request_stop();
        wxz::workstation::RpcService::Reply rep;
        rep.status = wxz::workstation::Status::ok_status();
        rep.result = Json{{"requested", true}};
        return rep;
    });

    if (!rpc_server->start(&logger)) return nullptr;
    return rpc_server;
}

}  // namespace wxz::workstation::bt_service
