#include "internal/rpc_control_plane.h"

#include <string>

#include "internal/arm_rpc_handlers.h"
#include "internal/arm_rpc_service_config.h"
#include "internal/arm_command_processor.h"

#include "logger.h"
#include "strand.h"

#include "workstation/arm_control_rpc.h"
#include "workstation/node.h"
#include "workstation/service.h"

namespace wxz::workstation::arm_control::internal {

std::unique_ptr<wxz::workstation::RpcService> start_arm_rpc_control_plane(bool enable,
                                                                          int domain_id,
                                                                          const std::string& request_topic,
                                                                          const std::string& reply_topic,
                                                                          const std::string& service_name,
                                                                          const std::string& sw_version,
                                                                          ArmCommandProcessor& processor,
                                                                          IArmClient& arm,
                                                                          wxz::core::Strand& arm_sdk_strand,
                                                                          wxz::core::Logger& logger) {
    if (!enable) return nullptr;

    auto rpc_server = std::make_unique<wxz::workstation::RpcService>(make_arm_rpc_service_config(
        domain_id,
        request_topic,
        reply_topic,
        service_name,
        sw_version));

    // 与 SDK 工作串行化，避免并发访问 arm client。
    rpc_server->bind_scheduler(arm_sdk_strand);

    install_arm_rpc_handlers(*rpc_server, processor, arm, logger);

    if (!rpc_server->start(&logger)) {
        return nullptr;
    }
    return rpc_server;
}

std::unique_ptr<wxz::workstation::RpcService> start_arm_rpc_control_plane(const ArmControlConfig& cfg,
                                                                          wxz::workstation::Node& node,
                                                                          ArmCommandProcessor& processor,
                                                                          IArmClient& arm,
                                                                          wxz::core::Strand& arm_sdk_strand,
                                                                          wxz::core::Logger& logger) {
    if (!cfg.rpc_enable) return nullptr;

    auto opts_builder = wxz::workstation::RpcService::Options::builder(cfg.rpc_service_name);
    opts_builder.domain(cfg.domain)
        .sw_version(cfg.sw_version)
        .request_topic(cfg.rpc_req_topic)
        .reply_topic(cfg.rpc_rep_topic)
        .metrics_scope(cfg.metrics_scope);

    auto rpc_server = node.create_service_on(arm_sdk_strand, std::move(opts_builder).build());

    install_arm_rpc_handlers(*rpc_server, processor, arm, logger);

    if (!rpc_server->start(&logger)) {
        return nullptr;
    }
    return rpc_server;
}

std::unique_ptr<wxz::workstation::RpcService> start_arm_rpc_control_plane(const ArmControlConfig& cfg,
                                                                          ArmCommandProcessor& processor,
                                                                          IArmClient& arm,
                                                                          wxz::core::Strand& arm_sdk_strand,
                                                                          wxz::core::Logger& logger) {
    return start_arm_rpc_control_plane(cfg.rpc_enable,
                                       cfg.domain,
                                       cfg.rpc_req_topic,
                                       cfg.rpc_rep_topic,
                                       cfg.rpc_service_name,
                                       cfg.sw_version,
                                       processor,
                                       arm,
                                       arm_sdk_strand,
                                       logger);
}

}  // namespace wxz::workstation::arm_control::internal
