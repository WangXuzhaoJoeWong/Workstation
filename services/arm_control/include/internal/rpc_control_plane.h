#pragma once

#include <memory>
#include <string>

#include "internal/arm_control_config.h"
#include "workstation/service.h"

namespace wxz::core {
class Logger;
class Strand;
}  // namespace wxz::core

namespace wxz::workstation::arm_control {
namespace internal {
class IArmClient;
class ArmCommandProcessor;

/// 启动 arm_control 的可选 RPC 控制面。
///
/// 线程模型：RPC handler 会把实际 SDK 调用投递到 arm_sdk_strand，以避免并发访问 arm client。
///
/// 注意：该重载为 legacy 形态（参数较多，易传错）。
/// 推荐优先使用下方的 `start_arm_rpc_control_plane(const ArmControlConfig&, ...)`。
std::unique_ptr<wxz::workstation::RpcService> start_arm_rpc_control_plane(bool enable,
                                                                          int domain_id,
                                                                          const std::string& request_topic,
                                                                          const std::string& reply_topic,
                                                                          const std::string& service_name,
                                                                          const std::string& sw_version,
                                                                          ArmCommandProcessor& processor,
                                                                          IArmClient& arm,
                                                                          wxz::core::Strand& arm_sdk_strand,
                                                                          wxz::core::Logger& logger);

/// start_arm_rpc_control_plane 的便捷重载：从 ArmControlConfig 读取 enable/domain/topic/name/version。
std::unique_ptr<wxz::workstation::RpcService> start_arm_rpc_control_plane(const ArmControlConfig& cfg,
                                                                          ArmCommandProcessor& processor,
                                                                          IArmClient& arm,
                                                                          wxz::core::Strand& arm_sdk_strand,
                                                                          wxz::core::Logger& logger);

}  // namespace internal
}  // namespace wxz::workstation::arm_control
