#include "internal/rpc_kv_codec.h"

#include <cstddef>
#include <utility>

namespace wxz::workstation::arm_control::internal {

std::string json_to_scalar(const Json& v) {
    if (v.is_string()) return v.get<std::string>();
    if (v.is_boolean()) return v.get<bool>() ? "1" : "0";
    if (v.is_number_integer()) return std::to_string(v.get<long long>());
    if (v.is_number_unsigned()) return std::to_string(v.get<unsigned long long>());
    if (v.is_number_float()) return std::to_string(v.get<double>());
    return v.dump();
}

std::string json_to_csv(const Json& arr) {
    std::string out;
    for (std::size_t i = 0; i < arr.size(); ++i) {
        if (i) out.push_back(',');
        out += json_to_scalar(arr[i]);
    }
    return out;
}

std::optional<std::string> build_raw_kv_from_params(const Json& params) {
    if (!params.is_object()) return std::nullopt;
    auto it = params.find("op");
    if (it == params.end() || !it->is_string()) return std::nullopt;

    std::string raw;
    raw.reserve(256);
    raw += "op=";
    raw += it->get<std::string>();

    for (auto& [k, v] : params.items()) {
        if (k == "op") continue;
        raw.push_back(';');
        raw += k;
        raw.push_back('=');
        if (v.is_array()) {
            raw += json_to_csv(v);
        } else {
            raw += json_to_scalar(v);
        }
    }
    return raw;
}

} // namespace wxz::workstation::arm_control::internal
