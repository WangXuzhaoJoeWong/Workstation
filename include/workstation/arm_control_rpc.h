#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "framework/typed_rpc.h"
#include "workstation/service.h"

namespace wxz::workstation::arm_control::rpc {

inline constexpr std::string_view kService = "arm_control";
inline constexpr std::string_view kOpPing = "arm.ping";
inline constexpr std::string_view kOpCommand = "arm.command";

struct PingRequest {};

struct PingReply {
    std::string service;
    std::string sw_version;
    int domain{0};
    std::uint64_t ts_ms{0};
};

inline void to_json(nlohmann::json& j, const PingRequest&) {
    j = nlohmann::json::object();
}

inline void from_json(const nlohmann::json& j, PingReply& r) {
    r.service = j.value("service", "");
    r.sw_version = j.value("sw_version", "");
    r.domain = j.value("domain", 0);
    r.ts_ms = j.value("ts_ms", 0ULL);
}

// 约定：params 至少包含 {"op":"..."}；其余字段作为业务参数透传。
struct CommandRequest {
    std::string op;
    nlohmann::json args = nlohmann::json::object();
};

inline void from_json(const nlohmann::json& j, CommandRequest& r) {
    if (!j.is_object()) {
        r = CommandRequest{};
        return;
    }
    r.args = j;
    r.op = j.value("op", "");
}

struct CommandReply {
    nlohmann::json kv = nlohmann::json::object();
};

inline void to_json(nlohmann::json& j, const CommandReply& r) {
    j = nlohmann::json::object();
    j["kv"] = r.kv;
}

inline void to_json(nlohmann::json& j, const CommandRequest& r) {
    j = r.args.is_object() ? r.args : nlohmann::json::object();
    j["op"] = r.op;
}

inline void from_json(const nlohmann::json& j, CommandReply& r) {
    auto it = j.find("kv");
    if (it != j.end()) r.kv = *it;
}

// --- Convenience helpers (typed client calls) ---

inline wxz::framework::typed_rpc::Result<PingReply> ping(wxz::framework::RpcServiceClient& cli,
                                                        std::chrono::milliseconds timeout) {
    return wxz::framework::typed_rpc::call<PingRequest, PingReply>(
        cli, std::string(kOpPing), PingRequest{}, timeout);
}

inline wxz::framework::typed_rpc::Result<PingReply> ping(wxz::framework::RpcServiceClient& cli) {
    return wxz::framework::typed_rpc::call<PingRequest, PingReply>(
        cli, std::string(kOpPing), PingRequest{});
}

inline wxz::framework::typed_rpc::Result<CommandReply> command(wxz::framework::RpcServiceClient& cli,
                                                               CommandRequest req,
                                                               std::chrono::milliseconds timeout) {
    return wxz::framework::typed_rpc::call<CommandRequest, CommandReply>(
        cli, std::string(kOpCommand), std::move(req), timeout);
}

inline wxz::framework::typed_rpc::Result<CommandReply> command(wxz::framework::RpcServiceClient& cli,
                                                               CommandRequest req) {
    return wxz::framework::typed_rpc::call<CommandRequest, CommandReply>(
        cli, std::string(kOpCommand), std::move(req));
}

} // namespace wxz::workstation::arm_control::rpc
