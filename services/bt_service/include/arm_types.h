#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "dto/event_dto.h"

namespace wxz::workstation::bt_service {

/// 返回单调时钟毫秒时间戳（用于超时/节流等，不随系统时间跳变）。
std::uint64_t now_monotonic_ms();

/// 生成一个用于请求/事务的短 ID（用于日志与链路追踪）。
std::string make_id();

/// 读取文本文件到 out。
/// 返回 true 表示读取成功；失败时 out 内容未定义（由实现决定）。
bool load_text_file(const std::string& path, std::string& out);

/// 运行期 trace 上下文（跨请求传播 trace_id）。
struct TraceContext {
    std::mutex mu;
    std::string active_trace_id;

    /// 若 trace_id 非空则更新 active_trace_id。
    void set_if_nonempty(const std::string& trace_id);

    /// 获取当前 active_trace_id 的拷贝。
    std::string get();
};

/// 将 trace/request 相关字段写入 DTO KV。
void fill_trace_fields(EventDTOUtil::KvMap& kv, TraceContext* ctx, const std::string& request_id);

/// 机械臂响应的归一化表示（从 DTO KV 中提取并保留原始 KV）。
struct ArmResp {
    std::string ok;
    std::string code;
    std::string err_code;
    std::string err;
    std::string sdk_code;
    std::uint64_t ts_ms{0};
    EventDTOUtil::KvMap kv;
};

/// 以 request_id 为 key 的响应缓存（供 BT 节点查询）。
struct ArmRespCache {
    std::mutex mu;
    std::unordered_map<std::string, ArmResp> by_id;

    /// 写入一条响应（覆盖同 id 的旧值）。
    void put(const std::string& id, ArmResp r);

    /// 读取一条响应；不存在时返回 std::nullopt。
    std::optional<ArmResp> get(const std::string& id);
};

/// 在 ok 与 err_code 表达冲突时，优先采用 err_code 的成功语义。
bool prefer_err_code_success(const std::string& ok, const std::string& err_code);

/// 将字符串按“真值”解析（如 "1"/"true" 等）。
bool is_truthy(const std::string& v);

/// 从 KV 中读取 key；不存在则返回 def。
std::string kv_get_or(const EventDTOUtil::KvMap& kv, const char* key, const std::string& def);

}  // namespace wxz::workstation::bt_service
