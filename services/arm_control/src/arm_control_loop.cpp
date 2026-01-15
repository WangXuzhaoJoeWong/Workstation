#include "internal/arm_control_loop.h"

#include <algorithm>
#include <utility>

#include "internal/arm_command_processor.h"
#include "internal/arm_control_internal.h"
#include "internal/arm_error_codes.h"

#include "executor.h"
#include "service_common.h"
#include "strand.h"
#include "workstation/node.h"

namespace wxz::workstation::arm_control::internal {

namespace {

struct Kv {
    static std::string get(const EventDTOUtil::KvMap& kv, const char* key) {
        auto it = kv.find(key);
        if (it == kv.end()) return {};
        return it->second;
    }

    static int get_int(const EventDTOUtil::KvMap& kv, const char* key, int def) {
        auto s = get(kv, key);
        if (s.empty()) return def;
        try {
            return std::stoi(s);
        } catch (...) {
            return def;
        }
    }
};

} // namespace

ArmControlLoop::ArmControlLoop(wxz::workstation::Node& node,
                               wxz::core::Executor& exec,
                               wxz::core::Strand& arm_sdk_strand,
                               ArmCommandProcessor& processor,
                               IArmClient& arm,
                               CmdQueue& queue,
                               wxz::workstation::EventDtoPublisher& status_pub,
                               ArmControlTopics topics,
                               Options opts,
                               wxz::core::Logger& logger)
    : node_(node)
    , exec_(exec)
    , arm_sdk_strand_(arm_sdk_strand)
    , processor_(processor)
    , arm_(arm)
    , queue_(queue)
    , status_pub_(status_pub)
    , topics_(std::move(topics))
    , opts_(std::move(opts))
    , logger_(logger) {}

void ArmControlLoop::run(std::chrono::milliseconds pop_timeout) {
    wxz::core::ChannelQoS qos = wxz::core::default_reliable_qos();

    // 跨线程移交：
    // - DDS 回调线程只做轻量入队（请求/结果）。
    // - 只有本循环会调用 NodeBase / status 发布器。
    // - SDK 调用在串行的 arm_sdk_strand 上执行。
    MpscQueue<EventDTOUtil::KvMap> resp_out_q;
    MpscQueue<wxz::core::FaultStatus> fault_out_q;
    MpscQueue<EventDTOUtil::KvMap> fault_action_q;

    auto maybe_publish_fault_from_resp = [&](const EventDTOUtil::KvMap& resp) {
        // 优先使用新字段：ok/err_code/err；同时兼容历史字段 ok/code。
        const std::string ok_s = Kv::get(resp, "ok");
        const int err_code = Kv::get_int(resp, "err_code", 0);
        const std::string err = Kv::get(resp, "err");
        const std::string sdk_code = Kv::get(resp, "sdk_code");

        bool ok = true;
        if (!ok_s.empty()) {
            ok = (ok_s == "1" || ok_s == "true" || ok_s == "TRUE");
        }
        if (!ok || err_code != 0) {
            wxz::core::FaultStatus st;
            st.fault = "arm.command";
            st.active = true;
            st.severity = "error";
            st.err_code = err_code != 0 ? err_code : Kv::get_int(resp, "code", 1);
            st.err = err;
            if (!sdk_code.empty()) {
                if (!st.err.empty()) st.err += " ";
                st.err += "(sdk_code=" + sdk_code + ")";
            }
            if (!node_.base().publish_fault(std::move(st))) {
                logger_.log(LogLevel::Warn, "fault publish skipped (fault_topic not configured)");
            }
        }
    };

    auto publish_status_kv = [&](const EventDTOUtil::KvMap& kv) {
        ::EventDTO dto;
        dto.version = 1;
        dto.schema_id = topics_.status_dto_schema;
        dto.topic = topics_.status_dto_topic;
        dto.payload = EventDTOUtil::buildPayloadKv(kv);
        EventDTOUtil::fillMeta(dto, topics_.dto_source);
        if (auto it = kv.find("id"); it != kv.end() && !it->second.empty()) {
            dto.event_id = it->second;
        }
        if (!status_pub_.publish(dto)) {
            logger_.log(LogLevel::Warn, "status publish failed");
        }
    };

    wxz::workstation::EventDtoSubscription::Options cmd_sub_opts;
    cmd_sub_opts.qos = qos;
    cmd_sub_opts.dto_max_payload = topics_.dto_max_payload;
    cmd_sub_opts.pool_buffers = Env::get_size("WXZ_CMD_INGRESS_POOL_BUFFERS", std::max<std::size_t>(64, opts_.queue_max * 2));
    cmd_sub_opts.metrics_scope = opts_.metrics_scope;

    auto cmd_sub = node_.create_subscription_eventdto(
        topics_.cmd_dto_topic,
        topics_.cmd_dto_schema,
        [&](const ::EventDTO& dto) {
            if (!queue_.push(Cmd{dto.payload})) {
                logger_.log(LogLevel::Warn, "queue full, drop cmd");
                resp_out_q.push({
                    {"ok", "0"},
                    {"code", std::to_string(static_cast<int>(ArmErrc::QueueFull))},
                    {"err", "queue_full"},
                    {"err_code", std::to_string(static_cast<int>(ArmErrc::QueueFull))},
                });

                wxz::core::FaultStatus st;
                st.fault = "arm.queue_full";
                st.active = true;
                st.severity = "warn";
                st.err_code = static_cast<int>(ArmErrc::QueueFull);
                st.err = "queue_full";
                fault_out_q.push(std::move(st));
            }
        },
        cmd_sub_opts);

    wxz::workstation::TextSubscription::Options fault_action_opts;
    fault_action_opts.qos = qos;
    fault_action_opts.max_payload = 2048;
    fault_action_opts.metrics_scope = opts_.metrics_scope;

    auto fault_action_sub = node_.create_subscription_text(
        topics_.fault_action_topic,
        [&](std::string raw) {
            auto kv = EventDTOUtil::parsePayloadKv(raw);

            const std::string target = kv["target"];
            const std::string action = kv["action"];
            if (target != opts_.metrics_scope) return;

            if (action == "reset") {
                logger_.log(LogLevel::Info, "fault/action reset received");

                // 主循环线程会发布 ack，并把 SDK 工作投递到 arm_sdk_strand。
                fault_action_q.push(std::move(kv));
            }
        },
        fault_action_opts);

    (void)fault_action_sub;

    auto drain_fault_out = [&] {
        wxz::core::FaultStatus st;
        while (fault_out_q.try_pop(st)) {
            if (!node_.base().publish_fault(std::move(st))) {
                logger_.log(LogLevel::Warn, "fault publish skipped (fault_topic not configured)");
            }
        }
    };

    auto drain_resp_out = [&] {
        EventDTOUtil::KvMap resp;
        while (resp_out_q.try_pop(resp)) {
            maybe_publish_fault_from_resp(resp);
            publish_status_kv(resp);
        }
    };

    auto handle_fault_actions = [&] {
        EventDTOUtil::KvMap req;
        while (fault_action_q.try_pop(req)) {
            wxz::core::FaultStatus ack;
            ack.fault = "arm.fault_reset";
            ack.active = false;
            ack.severity = "info";
            ack.err_code = 0;
            ack.err = "fault_reset_requested";
            if (!node_.base().publish_fault(ack)) {
                logger_.log(LogLevel::Warn, "fault/status ack publish failed");
            }

            const bool queued = arm_sdk_strand_.post([
                &arm = arm_,
                &logger = logger_,
                &fault_out_q,
                req = std::move(req)
            ]() mutable {
                const auto r = arm.fault_reset();

                wxz::core::FaultStatus st;
                st.fault = req.count("fault") ? req["fault"] : std::string("arm.fault");
                if (r == 0) {
                    st.active = false;
                    st.severity = "info";
                    st.err_code = 0;
                    st.err = "fault_reset_ok";
                    logger.log(LogLevel::Info, "fault_reset ok");
                } else {
                    st.active = true;
                    st.severity = "error";
                    st.err_code = static_cast<int>(r);
                    st.err = "fault_reset_failed";
                    logger.log(LogLevel::Warn, "fault_reset failed code=" + std::to_string(static_cast<int>(r)));
                }
                fault_out_q.push(std::move(st));
            });

            if (!queued) {
                logger_.log(LogLevel::Warn, "fault_reset dropped: arm_sdk_strand rejected task");
            }
        }
    };

    auto dispatch_one_cmd = [&] {
        if (auto cmd_opt = queue_.try_pop(); cmd_opt) {
            const bool queued = arm_sdk_strand_.post([
                &processor = processor_,
                &arm = arm_,
                &logger = logger_,
                &resp_out_q,
                cmd_raw = std::move(cmd_opt->raw)
            ]() mutable {
                EventDTOUtil::KvMap resp = processor.handle_raw_command(cmd_raw, arm, logger);
                resp_out_q.push(std::move(resp));
            });
            if (!queued) {
                logger_.log(LogLevel::Warn, "cmd dropped: arm_sdk_strand rejected task");
                resp_out_q.push({
                    {"ok", "0"},
                    {"err", "executor_rejected"},
                    {"code", std::to_string(static_cast<int>(ArmErrc::InvalidArgs))},
                    {"err_code", std::to_string(static_cast<int>(ArmErrc::InvalidArgs))},
                });
            }
        }
    };

    (void)cmd_sub;

    const auto spin_slice = (pop_timeout > std::chrono::milliseconds(5)) ? std::chrono::milliseconds(5) : pop_timeout;

    while (node_.base().running()) {
        node_.base().tick();

        // 发布 DDS 回调 / strand 产生的结果。
        drain_fault_out();
        drain_resp_out();

        // 处理 fault action（主线程发 ack，SDK 调用在 strand 上执行）。
        handle_fault_actions();

        // 每轮最多取出并派发一条命令。
        dispatch_one_cmd();

        // 每轮驱动一个待执行的回调/任务（ingress + sdk + rpc，如它们绑定到该 executor）。
        (void)exec_.spin_once(spin_slice);
    }
}

} // namespace wxz::workstation::arm_control::internal
