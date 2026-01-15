#pragma once

#include <optional>
#include <string>

#include "workstation/service.h"

namespace wxz::workstation::arm_control::internal {

using Json = wxz::workstation::RpcService::Json;

std::string json_to_scalar(const Json& v);

std::string json_to_csv(const Json& arr);

// 将 JSON-RPC 的 params 对象转换为原始 kv 字符串，例如："op=...;k=v;arr=1,2"。
// 要求：params 为 object，且 params["op"] 为 string。
std::optional<std::string> build_raw_kv_from_params(const Json& params);

} // namespace wxz::workstation::arm_control::internal
