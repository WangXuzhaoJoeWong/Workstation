#pragma once

#include <cstddef>
#include <cstdint>

namespace wxz::core {
class ByteBufferPool;
class FastddsChannel;
class Strand;
}

namespace wxz::workstation::bt_service {

class ArmRespCache;

/// 安装机械臂 status DTO 的订阅回调，并将结果写入 ArmRespCache。
///
/// 并发约束：FastDDS listener 线程不执行业务逻辑；回调应通过 ingress_strand 串行化。
void install_arm_status_cache_updater(wxz::core::FastddsChannel& arm_status_dto_sub,
									  wxz::core::ByteBufferPool& ingress_pool,
									  wxz::core::Strand& ingress_strand,
									  ArmRespCache& arm_cache);

}  // namespace wxz::workstation::bt_service
