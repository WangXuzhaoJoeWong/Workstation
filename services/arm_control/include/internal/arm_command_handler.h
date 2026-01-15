#pragma once

#include <string>

#include "internal/arm_control_internal.h"

namespace wxz::workstation::arm_control::internal {

/// 机械臂指令（从 EventDTO.payload 的 KV 解析得到）。
struct ArmCommand {
    std::string raw;
    EventDTOUtil::KvMap kv;
    std::string op;
    std::string id;
};

/// 解析原始 KV 字符串为 ArmCommand。
ArmCommand parse_arm_command(const std::string& raw);

/// 指令处理函数签名。
using ArmCommandHandler = EventDTOUtil::KvMap (*)(const ArmCommand& cmd, IArmClient& arm, const Logger& logger);

/// 为指定操作名注册 handler（例如 "moveL"）。
void register_arm_handler(const std::string& op, ArmCommandHandler fn);

/// 初始化内置操作的默认 handlers。
void init_default_arm_handlers();

/// 处理一条指令并返回 KV 响应负载（将被封装到 DTO 后发布到 /arm/status）。
EventDTOUtil::KvMap handle_arm_command(const ArmCommand& cmd, IArmClient& arm, const Logger& logger);

} // namespace wxz::workstation::arm_control::internal
