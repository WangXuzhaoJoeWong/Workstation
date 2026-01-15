#include "internal/arm_rpc_handlers.h"

#include <string>

#include "framework/typed_rpc.h"
#include "internal/arm_command_processor.h"
#include "internal/arm_control_internal.h"
#include "internal/rpc_kv_codec.h"
#include "workstation/arm_control_rpc.h"

namespace wxz::workstation::arm_control::internal {

void install_arm_rpc_handlers(wxz::workstation::RpcService& rpc_server,
                              ArmCommandProcessor& processor,
                              IArmClient& arm,
                              wxz::core::Logger& logger) {
    using Json = wxz::workstation::RpcService::Json;

    rpc_server.add_ping_handler("arm.ping");

    // 通用命令入口：params 至少需要包含 {"op":"..."}。
    // 其它字段会被转换为 KV，并复用既有的领域处理逻辑。
    // typed wrapper 仅提升可读性：不改变底层 DTO/RPC 与 wire format。
    wxz::framework::typed_rpc::add_handler<wxz::workstation::arm_control::rpc::CommandRequest,
                                          wxz::workstation::arm_control::rpc::CommandReply>(
        rpc_server,
        std::string(wxz::workstation::arm_control::rpc::kOpCommand),
        [&](const wxz::workstation::arm_control::rpc::CommandRequest& req)
            -> wxz::framework::typed_rpc::Result<wxz::workstation::arm_control::rpc::CommandReply> {
            const auto raw_opt = build_raw_kv_from_params(req.args);
            if (!raw_opt || req.op.empty()) {
                wxz::framework::typed_rpc::Result<wxz::workstation::arm_control::rpc::CommandReply> out;
                out.status = wxz::workstation::Status::error(1, "missing_or_invalid_params.op");
                return out;
            }

            EventDTOUtil::KvMap kv = processor.handle_raw_command(*raw_opt, arm, logger);

            Json kv_json = Json::object();
            for (const auto& [k, v] : kv) {
                kv_json[k] = v;
            }

            // 保持历史行为：RPC 传输/handler 成功用 ok=true 表示；业务失败信息由 kv 内字段承载。
            wxz::framework::typed_rpc::Result<wxz::workstation::arm_control::rpc::CommandReply> out;
            out.status = wxz::workstation::Status::ok_status();
            out.value.kv = std::move(kv_json);
            return out;
        });
}

} // namespace wxz::workstation::arm_control::internal
