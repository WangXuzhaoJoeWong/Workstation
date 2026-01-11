#pragma once

#include <string>
#include <string_view>

#include "dto/event_dto.h"

namespace wxz::workstation::arm_control::internal {

// arm/status error code conventions.
//
// Compatibility:
// - Keep existing fields: ok, code (historically SDK CRresult integer)
// - Add new optional fields:
//   - err_code: stable integer code defined by this enum
//   - err: stable short string token (snake_case)
//   - sdk_code: raw SDK CRresult integer when applicable
//
// Suggested consumer logic:
// - ok=="1" => success
// - else check err_code/err; fall back to code/sdK_code if needed.

enum class ArmErrc : int {
    Ok = 0,

    // Request/dispatch layer
    BadRequest = 1001,
    MissingField = 1002,
    ParseError = 1003,
    QueueFull = 1101,
    UnknownOp = 1102,

    // SDK layer
    SdkUnavailable = 2002,
    SdkCallFailed = 2001,

    // Generic
    InternalError = 9000,
};

inline void arm_set_ok(EventDTOUtil::KvMap& resp) {
    resp["ok"] = "1";
    resp["err_code"] = std::to_string(static_cast<int>(ArmErrc::Ok));
    resp.erase("err");
}

inline void arm_set_error(EventDTOUtil::KvMap& resp, ArmErrc code, std::string_view err) {
    resp["ok"] = "0";
    resp["err_code"] = std::to_string(static_cast<int>(code));
    resp["err"] = std::string(err);
}

inline void arm_set_sdk_result(EventDTOUtil::KvMap& resp, int sdk_code) {
    // Historical field: code = raw SDK code.
    resp["code"] = std::to_string(sdk_code);
    resp["sdk_code"] = std::to_string(sdk_code);
    if (sdk_code == 0) {
        arm_set_ok(resp);
    } else {
        arm_set_error(resp, ArmErrc::SdkCallFailed, "sdk_call_failed");
    }
}

} // namespace wxz::workstation::arm_control::internal
