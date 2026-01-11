#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "dto/event_dto.h"

namespace wxz::workstation::bt_service {

std::uint64_t now_monotonic_ms();
std::string make_id();

bool load_text_file(const std::string& path, std::string& out);

struct TraceContext {
    std::mutex mu;
    std::string active_trace_id;

    void set_if_nonempty(const std::string& trace_id);
    std::string get();
};

void fill_trace_fields(EventDTOUtil::KvMap& kv, TraceContext* ctx, const std::string& request_id);

struct ArmResp {
    std::string ok;
    std::string code;
    std::string err_code;
    std::string err;
    std::string sdk_code;
    std::uint64_t ts_ms{0};
    EventDTOUtil::KvMap kv;
};

struct ArmRespCache {
    std::mutex mu;
    std::unordered_map<std::string, ArmResp> by_id;

    void put(const std::string& id, ArmResp r);
    std::optional<ArmResp> get(const std::string& id);
};

bool prefer_err_code_success(const std::string& ok, const std::string& err_code);
bool is_truthy(const std::string& v);
std::string kv_get_or(const EventDTOUtil::KvMap& kv, const char* key, const std::string& def);

}  // namespace wxz::workstation::bt_service
