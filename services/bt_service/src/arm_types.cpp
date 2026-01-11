#include "wxz_workstation/bt_service/arm_types.h"

#include <chrono>
#include <fstream>
#include <iterator>
#include <random>

namespace wxz::workstation::bt_service {

std::uint64_t now_monotonic_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::string make_id() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    const std::uint64_t t = now_monotonic_ms();
    const std::uint64_t r = rng();
    return std::to_string(t) + "-" + std::to_string(r);
}

bool load_text_file(const std::string& path, std::string& out) {
    std::ifstream ifs(path);
    if (!ifs) return false;
    out.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    return true;
}

void TraceContext::set_if_nonempty(const std::string& trace_id) {
    if (trace_id.empty()) return;
    std::lock_guard<std::mutex> lock(mu);
    active_trace_id = trace_id;
}

std::string TraceContext::get() {
    std::lock_guard<std::mutex> lock(mu);
    return active_trace_id;
}

void fill_trace_fields(EventDTOUtil::KvMap& kv, TraceContext* ctx, const std::string& request_id) {
    if (!ctx) return;
    const std::string trace_id = ctx->get();
    if (!trace_id.empty()) kv["trace_id"] = trace_id;
    if (!request_id.empty()) kv["request_id"] = request_id;
}

void ArmRespCache::put(const std::string& id, ArmResp r) {
    std::lock_guard<std::mutex> lock(mu);
    by_id[id] = std::move(r);
    if (by_id.size() > 256) {
        const std::uint64_t cutoff = now_monotonic_ms() - 30'000;
        for (auto it = by_id.begin(); it != by_id.end();) {
            if (it->second.ts_ms < cutoff) it = by_id.erase(it);
            else ++it;
        }
    }
}

std::optional<ArmResp> ArmRespCache::get(const std::string& id) {
    std::lock_guard<std::mutex> lock(mu);
    auto it = by_id.find(id);
    if (it == by_id.end()) return std::nullopt;
    return it->second;
}

bool prefer_err_code_success(const std::string& ok, const std::string& err_code) {
    if (!err_code.empty()) {
        return err_code == "0";
    }
    return ok == "1";
}

bool is_truthy(const std::string& v) {
    return (v == "1" || v == "true" || v == "TRUE" || v == "yes" || v == "YES");
}

std::string kv_get_or(const EventDTOUtil::KvMap& kv, const char* key, const std::string& def) {
    auto it = kv.find(key);
    if (it == kv.end()) return def;
    return it->second;
}

}  // namespace wxz::workstation::bt_service
