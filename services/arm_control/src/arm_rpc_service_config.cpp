#include "internal/arm_rpc_service_config.h"

#include <utility>

namespace wxz::workstation::arm_control::internal {

wxz::workstation::RpcService::Config make_arm_rpc_service_config(int domain_id,
                                                                 std::string request_topic,
                                                                 std::string reply_topic,
                                                                 std::string service_name,
                                                                 std::string sw_version) {
    wxz::workstation::RpcService::Config cfg;
    cfg.domain = domain_id;
    cfg.service_name = std::move(service_name);
    cfg.request_topic = std::move(request_topic);
    cfg.reply_topic = std::move(reply_topic);
    cfg.sw_version = std::move(sw_version);
    return cfg;
}

} // namespace wxz::workstation::arm_control::internal
