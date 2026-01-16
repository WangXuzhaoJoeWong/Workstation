#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "workstation/node.h"

namespace wxz::core {
class Strand;
}

namespace wxz::workstation::bt_service {

class ArmRespCache;

/// 安装机械臂 status DTO 的订阅回调，并将结果写入 ArmRespCache。
///
/// 并发约束：回调通过 ingress_strand 串行化。
///
/// 返回订阅句柄；调用方必须持有该对象以维持订阅生命周期。
std::unique_ptr<wxz::workstation::EventDtoSubscription> install_arm_status_cache_updater(
	wxz::workstation::Node& node,
	const std::string& status_dto_topic,
	const std::string& status_dto_schema,
	wxz::core::Strand& ingress_strand,
	std::size_t dto_max_payload,
	std::size_t pool_buffers,
	ArmRespCache& arm_cache);

}  // namespace wxz::workstation::bt_service
