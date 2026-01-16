#pragma once

#include <cstddef>
#include <memory>

#include "workstation/node.h"

namespace BT {
class BehaviorTreeFactory;
}

namespace wxz::workstation::bt_service {

struct AppConfig;
struct DdsChannels;
class ArmRespCache;
struct TraceContext;

}  // namespace wxz::workstation::bt_service

namespace wxz::core {
class Strand;
}

namespace wxz::workstation::bt_service {

/// 将 arm_control 相关功能“接线”到 BT 系统：
/// - 创建/注册 BT 节点
/// - 订阅 arm status 并写入缓存
/// - 配置 trace 上下文与超时
std::unique_ptr<wxz::workstation::EventDtoSubscription> setup_arm_control_bt(
    BT::BehaviorTreeFactory& factory,
    const AppConfig& cfg,
    wxz::workstation::Node& node,
    DdsChannels& channels,
    wxz::core::Strand& arm_status_ingress_strand,
    std::size_t arm_status_pool_buffers,
    ArmRespCache& arm_cache,
    TraceContext& trace_ctx);

}  // namespace wxz::workstation::bt_service
