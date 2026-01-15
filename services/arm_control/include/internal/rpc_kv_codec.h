#pragma once

#include <optional>
#include <string>

#include "workstation/service.h"

namespace wxz::workstation::arm_control::internal {

using Json = wxz::workstation::RpcService::Json;

std::string json_to_scalar(const Json& v);

std::string json_to_csv(const Json& arr);

// Convert JSON-RPC params object into raw kv string, e.g. "op=...;k=v;arr=1,2".
// Requires params is object and params["op"] is string.
std::optional<std::string> build_raw_kv_from_params(const Json& params);

} // namespace wxz::workstation::arm_control::internal
