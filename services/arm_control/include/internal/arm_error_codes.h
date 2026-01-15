#pragma once

#include <string>
#include <string_view>

#include "dto/event_dto.h"

namespace wxz::workstation::arm_control::internal {

// /arm/status 错误码约定。
//
// 兼容性：
// - 保留既有字段：ok、code（历史上等同 SDK CRresult 整数）
// - 新增可选字段：
//   - err_code：由本枚举定义的稳定整数码
//   - err：稳定的短字符串 token（snake_case）
//   - sdk_code：必要时携带原始 SDK CRresult 整数
//
// 建议消费端逻辑：
// - ok=="1" 表示成功
// - 否则优先看 err_code/err；必要时再回退到 code/sdk_code

enum class ArmErrc : int {
    Ok = 0,

    // 请求/派发层
    BadRequest = 1001,
    MissingField = 1002,
    ParseError = 1003,
    InvalidArgs = 1004,
    QueueFull = 1101,
    UnknownOp = 1102,

    // SDK 层
    SdkUnavailable = 2002,
    SdkCallFailed = 2001,

    // 通用
    InternalError = 9000,
};

/// 将 resp 设置为成功，并填充 err_code/err 字段。
inline void arm_set_ok(EventDTOUtil::KvMap& resp) {
    resp["ok"] = "1";
    resp["err_code"] = std::to_string(static_cast<int>(ArmErrc::Ok));
    resp.erase("err");
}

/// 将 resp 设置为失败，并写入 err_code/err。
inline void arm_set_error(EventDTOUtil::KvMap& resp, ArmErrc code, std::string_view err) {
    resp["ok"] = "0";
    resp["err_code"] = std::to_string(static_cast<int>(code));
    resp["err"] = std::string(err);
}

/// 写入 SDK 返回码字段，并同步设置 ok/err_code/err。
inline void arm_set_sdk_result(EventDTOUtil::KvMap& resp, int sdk_code) {
    // 历史字段：code 等同 raw SDK code。
    resp["code"] = std::to_string(sdk_code);
    resp["sdk_code"] = std::to_string(sdk_code);
    if (sdk_code == 0) {
        arm_set_ok(resp);
    } else {
        arm_set_error(resp, ArmErrc::SdkCallFailed, "sdk_call_failed");
    }
}

} // namespace wxz::workstation::arm_control::internal
