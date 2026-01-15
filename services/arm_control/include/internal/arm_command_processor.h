#pragma once

#include <string>

#include "dto/event_dto.h"
#include "logger.h"

namespace wxz::workstation::arm_control::internal {

class IArmClient;

/// arm_control 的业务入口：负责将输入指令（raw KV）处理为输出状态（KV map）。
///
/// - 输入：raw KV 字符串（来自 EventDTO.payload）
/// - 输出：用于发布到 /arm/status 的 KV map
class ArmCommandProcessor {
public:
    /// 处理一条原始指令。
    EventDTOUtil::KvMap handle_raw_command(const std::string& raw,
                                          IArmClient& arm,
                                          const wxz::core::Logger& logger) const;
};

}  // namespace wxz::workstation::arm_control::internal
