#pragma once

#include <memory>

#include "workstation/node.h"
#include "workstation/service.h"

namespace wxz::core {
class Logger;
class Strand;
}  // namespace wxz::core

namespace wxz::workstation::bt_service {

struct AppConfig;
class BtTreeRunner;

/// 启动 bt_service 的可选 RPC 控制面。
///
/// - 当配置禁用（enable=0）或 start 失败时，返回 nullptr。
/// - 返回的 server 必须在其依赖对象析构前停止（stop）。
std::unique_ptr<wxz::workstation::RpcService> start_bt_rpc_control_plane(const AppConfig& cfg,
                                                                      wxz::workstation::Node& node,
                                                                      BtTreeRunner& tree_runner,
                                                                      wxz::core::Strand& rpc_strand,
                                                                      wxz::core::Logger& logger);

}  // namespace wxz::workstation::bt_service
