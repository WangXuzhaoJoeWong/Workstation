#include "wxz_workstation/arm_control/internal/adapters/arm_control_dds_adapter.h"

#include <string>

#include "wxz_workstation/arm_control/internal/arm_control_internal.h"
#include "wxz_workstation/arm_control/internal/arm_error_codes.h"
#include "wxz_workstation/arm_control/internal/domain/arm_control_domain.h"

#include "dto/event_dto_cdr.h"

#include "fastdds_channel.h"
#include "fault_action.h"
#include "node_base.h"
#include "service_common.h"

namespace wxz::workstation::arm_control::adapters {

ArmControlDdsAdapter::ArmControlDdsAdapter(ArmControlTopics topics) : topics_(std::move(topics)) {}

bool ArmControlDdsAdapter::run(wxz::core::NodeBase& node,
                              internal::CmdQueue& queue,
                              internal::StatusPublisher& status_pub,
                              const domain::ArmControlDomain& domain,
                              internal::IArmClient& arm,
                              const wxz::core::Logger& logger,
                              std::chrono::milliseconds pop_timeout) {
    wxz::core::ChannelQoS qos = wxz::core::default_reliable_qos();

    auto kv_get = [](const EventDTOUtil::KvMap& kv, const char* key) -> std::string {
        auto it = kv.find(key);
        if (it == kv.end()) return {};
        return it->second;
    };

    auto kv_get_int = [&](const EventDTOUtil::KvMap& kv, const char* key, int def) -> int {
        auto s = kv_get(kv, key);
        if (s.empty()) return def;
        try {
            return std::stoi(s);
        } catch (...) {
            return def;
        }
    };

    auto maybe_publish_fault_from_resp = [&](const EventDTOUtil::KvMap& resp) {
        // Prefer new fields: ok/err_code/err; keep compatibility with legacy ok/code.
        const std::string ok_s = kv_get(resp, "ok");
        const int err_code = kv_get_int(resp, "err_code", 0);
        const std::string err = kv_get(resp, "err");
        const std::string sdk_code = kv_get(resp, "sdk_code");

        bool ok = true;
        if (!ok_s.empty()) {
            ok = (ok_s == "1" || ok_s == "true" || ok_s == "TRUE");
        }
        if (!ok || err_code != 0) {
            wxz::core::FaultStatus st;
            st.fault = "arm.command";
            st.active = true;
            st.severity = "error";
            st.err_code = err_code != 0 ? err_code : kv_get_int(resp, "code", 1);
            st.err = err;
            if (!sdk_code.empty()) {
                if (!st.err.empty()) st.err += " ";
                st.err += "(sdk_code=" + sdk_code + ")";
            }
            if (!node.publish_fault(std::move(st))) {
                logger.log(internal::LogLevel::Warn, "fault publish skipped (fault_topic not configured)");
            }
        }
    };

    wxz::core::FastddsChannel cmd_dto_sub(topics_.domain,
                                         topics_.cmd_dto_topic,
                                         qos,
                                         topics_.dto_max_payload,
                                         /*enable_pub=*/false,
                                         /*enable_sub=*/true);
    cmd_dto_sub.subscribe([&](const std::uint8_t* data, std::size_t size) {
        std::vector<std::uint8_t> buf(data, data + size);
        ::EventDTO dto;
        if (!wxz::dto::decode_event_dto_cdr(buf, dto)) {
            logger.log(internal::LogLevel::Warn, "drop cmd: decode_event_dto_cdr failed");
            return;
        }
        if (!topics_.cmd_dto_schema.empty() && dto.schema_id != topics_.cmd_dto_schema) {
            logger.log(internal::LogLevel::Warn,
                       "drop cmd: unexpected schema_id='" + dto.schema_id + "' expected='" + topics_.cmd_dto_schema + "'");
            return;
        }

        if (!queue.push(internal::Cmd{dto.payload})) {
            logger.log(internal::LogLevel::Warn, "queue full, drop cmd");
            status_pub.publish_kv({
                {"ok", "0"},
                {"code", std::to_string(static_cast<int>(internal::ArmErrc::QueueFull))},
                {"err", "queue_full"},
                {"err_code", std::to_string(static_cast<int>(internal::ArmErrc::QueueFull))},
            });

            wxz::core::FaultStatus st;
            st.fault = "arm.queue_full";
            st.active = true;
            st.severity = "warn";
            st.err_code = static_cast<int>(internal::ArmErrc::QueueFull);
            st.err = "queue_full";
            (void)node.publish_fault(std::move(st));
        }
    });

    wxz::core::FastddsChannel fault_action_sub(topics_.domain, topics_.fault_action_topic, qos, 2048, /*enable_pub=*/false, /*enable_sub=*/true);
    fault_action_sub.subscribe([&](const std::uint8_t* data, std::size_t size) {
        std::string raw(reinterpret_cast<const char*>(data), size);
        auto kv = EventDTOUtil::parsePayloadKv(raw);

        const std::string target = kv["target"];
        const std::string action = kv["action"];
        if (target != "workstation_arm_control_service") return;

        if (action == "reset") {
            logger.log(internal::LogLevel::Info, "fault/action reset received");

            // Always emit an immediate ack so callers/tests don't depend on SDK latency.
            {
                wxz::core::FaultStatus ack;
                ack.fault = "arm.fault_reset";
                ack.active = false;
                ack.severity = "info";
                ack.err_code = 0;
                ack.err = "fault_reset_requested";
                if (!node.publish_fault(ack)) {
                    logger.log(internal::LogLevel::Warn, "fault/status ack publish failed");
                }
            }

            const auto r = arm.fault_reset();

            wxz::core::FaultStatus st;
            st.fault = kv.count("fault") ? kv["fault"] : std::string("arm.fault");
            if (r == 0) {
                st.active = false;
                st.severity = "info";
                st.err_code = 0;
                st.err = "fault_reset_ok";
                logger.log(internal::LogLevel::Info, "fault_reset ok");
            } else {
                st.active = true;
                st.severity = "error";
                st.err_code = static_cast<int>(r);
                st.err = "fault_reset_failed";
                logger.log(internal::LogLevel::Warn, "fault_reset failed code=" + std::to_string(static_cast<int>(r)));
            }
            if (!node.publish_fault(std::move(st))) {
                logger.log(internal::LogLevel::Warn, "fault/status result publish failed");
            }
        }
    });

    while (node.running()) {
        node.tick();

        auto cmd_opt = queue.pop_for(pop_timeout, [&] { return node.running(); });
        if (!cmd_opt) continue;

        EventDTOUtil::KvMap resp = domain.handle_raw_command(cmd_opt->raw, arm, logger);
        maybe_publish_fault_from_resp(resp);
        status_pub.publish_kv(resp);
    }

    return false;
}

} // namespace wxz::workstation::arm_control::adapters
