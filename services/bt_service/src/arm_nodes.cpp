#include "arm_nodes.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "fastdds_channel.h"
#include "service_common.h"
#include "dto/event_dto.h"
#include "dto/event_dto_cdr.h"
#include "arm_types.h"

namespace wxz::workstation::bt_service {
namespace {

class ArmMoveLAction : public BT::StatefulActionNode {
public:
    ArmMoveLAction(const std::string& name,
                   const BT::NodeConfiguration& config,
                   wxz::core::FastddsChannel* cmd_dto_pub,
                   std::string cmd_dto_topic,
                   std::string cmd_dto_schema,
                   wxz::core::FastddsChannel* alert_pub,
                   std::string alert_topic,
                   std::string alert_schema,
                   std::string dto_source,
                   ArmRespCache* resp_cache,
                   std::uint64_t timeout_ms,
                   TraceContext* trace_ctx)
        : BT::StatefulActionNode(name, config),
          cmd_dto_pub_(cmd_dto_pub),
          cmd_dto_topic_(std::move(cmd_dto_topic)),
          cmd_dto_schema_(std::move(cmd_dto_schema)),
          alert_pub_(alert_pub),
          alert_topic_(std::move(alert_topic)),
          alert_schema_(std::move(alert_schema)),
          dto_source_(std::move(dto_source)),
          resp_cache_(resp_cache),
          timeout_ms_(timeout_ms),
          trace_ctx_(trace_ctx) {}

    static BT::PortsList providedPorts() {
        return {
            BT::InputPort<std::string>("pose"),
            BT::InputPort<std::string>("jointpos"),
            BT::InputPort<std::string>("speed"),
            BT::InputPort<std::string>("acc"),
            BT::InputPort<std::string>("jerk"),
        };
    }

    BT::NodeStatus onStart() override {
        if (!cmd_dto_pub_ || !resp_cache_) return BT::NodeStatus::FAILURE;

        id_ = make_id();
        deadline_ms_ = now_monotonic_ms() + timeout_ms_;
        alert_sent_ = false;

        const std::string pose = getInput<std::string>("pose").value_or("");
        const std::string jointpos = getInput<std::string>("jointpos").value_or("");
        const std::string speed = getInput<std::string>("speed").value_or("30");
        const std::string acc = getInput<std::string>("acc").value_or("30");
        const std::string jerk = getInput<std::string>("jerk").value_or("60");

        if (pose.empty() || jointpos.empty()) {
            publish_alert_once("E_ARM_BAD_INPUT",
                               std::string("missing required input: ") + (pose.empty() ? "pose" : "jointpos"),
                               nullptr);
            return BT::NodeStatus::FAILURE;
        }

        EventDTOUtil::KvMap kv;
        kv["op"] = "moveL";
        kv["id"] = id_;
        fill_trace_fields(kv, trace_ctx_, id_);
        kv["pose"] = pose;
        kv["jointpos"] = jointpos;
        kv["speed"] = speed;
        kv["acc"] = acc;
        kv["jerk"] = jerk;

        if (!publish_cmd(kv)) return BT::NodeStatus::FAILURE;
        return BT::NodeStatus::RUNNING;
    }

    BT::NodeStatus onRunning() override {
        if (now_monotonic_ms() > deadline_ms_) {
            publish_alert_once("E_ARM_TIMEOUT", "timeout waiting for /arm/status", nullptr);
            return BT::NodeStatus::FAILURE;
        }
        auto r = resp_cache_->get(id_);
        if (!r) return BT::NodeStatus::RUNNING;
        if (prefer_err_code_success(r->ok, r->err_code)) return BT::NodeStatus::SUCCESS;
        publish_alert_once("E_ARM_EXEC_FAIL", "arm command failed", &(*r));
        return BT::NodeStatus::FAILURE;
    }

    void onHalted() override {
        id_.clear();
        deadline_ms_ = 0;
    }

private:
    wxz::core::FastddsChannel* cmd_dto_pub_{nullptr};
    std::string cmd_dto_topic_;
    std::string cmd_dto_schema_;

    wxz::core::FastddsChannel* alert_pub_{nullptr};
    std::string alert_topic_;
    std::string alert_schema_;
    std::string dto_source_;

    ArmRespCache* resp_cache_{nullptr};
    std::uint64_t timeout_ms_{30'000};
    TraceContext* trace_ctx_{nullptr};

    std::string id_;
    std::uint64_t deadline_ms_{0};
    bool alert_sent_{false};

    bool publish_cmd(const EventDTOUtil::KvMap& kv) {
        ::EventDTO dto;
        dto.version = 1;
        dto.schema_id = cmd_dto_schema_;
        dto.topic = cmd_dto_topic_;
        dto.payload = EventDTOUtil::buildPayloadKv(kv);
        EventDTOUtil::fillMeta(dto, dto_source_);
        dto.event_id = kv.count("id") ? kv.at("id") : "";

        std::vector<std::uint8_t> buf;
        if (!wxz::dto::encode_event_dto_cdr(dto, buf)) return false;
        (void)cmd_dto_pub_->publish(buf.data(), buf.size());
        return true;
    }

    void publish_alert_once(const std::string& error_code, const std::string& message, const ArmResp* resp) {
        if (alert_sent_) return;
        if (!alert_pub_) return;

        EventDTOUtil::KvMap kv;
        kv["alert_level"] = "ERROR";
        kv["node_name"] = name();
        kv["error_code"] = error_code;
        kv["message"] = message;
        kv["op"] = "moveL";
        kv["id"] = id_;
        kv["ts_ms"] = std::to_string(wxz::core::now_epoch_ms());
        fill_trace_fields(kv, trace_ctx_, id_);
        if (resp) {
            if (!resp->sdk_code.empty()) kv["sdk_code"] = resp->sdk_code;
            if (!resp->err_code.empty()) kv["arm_err_code"] = resp->err_code;
            if (!resp->err.empty()) kv["arm_err"] = resp->err;
            if (!resp->code.empty()) kv["arm_code"] = resp->code;
        }

        ::EventDTO dto;
        dto.version = 1;
        dto.schema_id = alert_schema_;
        dto.topic = alert_topic_;
        dto.payload = EventDTOUtil::buildPayloadKv(kv);
        EventDTOUtil::fillMeta(dto, dto_source_);
        dto.event_id = id_;

        std::vector<std::uint8_t> buf;
        if (wxz::dto::encode_event_dto_cdr(dto, buf)) {
            (void)alert_pub_->publish(buf.data(), buf.size());
            std::cerr << "[workstation_bt_service][INF] arm alert published code=" << error_code
                      << " op=moveL node=" << name() << " id=" << id_ << "\n";
        }
        alert_sent_ = true;
    }
};

class ArmPowerOnAction : public BT::StatefulActionNode {
public:
    ArmPowerOnAction(const std::string& name,
                     const BT::NodeConfiguration& config,
                     wxz::core::FastddsChannel* cmd_dto_pub,
                     std::string cmd_dto_topic,
                     std::string cmd_dto_schema,
                     wxz::core::FastddsChannel* alert_pub,
                     std::string alert_topic,
                     std::string alert_schema,
                     std::string dto_source,
                     ArmRespCache* resp_cache,
                     std::uint64_t timeout_ms,
                     TraceContext* trace_ctx)
        : BT::StatefulActionNode(name, config),
          cmd_dto_pub_(cmd_dto_pub),
          cmd_dto_topic_(std::move(cmd_dto_topic)),
          cmd_dto_schema_(std::move(cmd_dto_schema)),
          alert_pub_(alert_pub),
          alert_topic_(std::move(alert_topic)),
          alert_schema_(std::move(alert_schema)),
          dto_source_(std::move(dto_source)),
          resp_cache_(resp_cache),
          timeout_ms_(timeout_ms),
          trace_ctx_(trace_ctx) {}

    static BT::PortsList providedPorts() { return {}; }

    BT::NodeStatus onStart() override {
        if (!cmd_dto_pub_ || !resp_cache_) return BT::NodeStatus::FAILURE;

        id_ = make_id();
        deadline_ms_ = now_monotonic_ms() + timeout_ms_;
        alert_sent_ = false;

        EventDTOUtil::KvMap kv;
        kv["op"] = "power_on_enable";
        kv["id"] = id_;
        fill_trace_fields(kv, trace_ctx_, id_);

        if (!publish_cmd(kv)) return BT::NodeStatus::FAILURE;
        return BT::NodeStatus::RUNNING;
    }

    BT::NodeStatus onRunning() override {
        if (now_monotonic_ms() > deadline_ms_) {
            publish_alert_once("E_ARM_POWER_ON_TIMEOUT", "timeout waiting for /arm/status", nullptr);
            return BT::NodeStatus::FAILURE;
        }
        auto r = resp_cache_->get(id_);
        if (!r) return BT::NodeStatus::RUNNING;
        if (prefer_err_code_success(r->ok, r->err_code)) return BT::NodeStatus::SUCCESS;
        publish_alert_once("E_ARM_POWER_ON_FAIL", "arm power_on_enable failed", &(*r));
        return BT::NodeStatus::FAILURE;
    }

    void onHalted() override {
        id_.clear();
        deadline_ms_ = 0;
    }

private:
    wxz::core::FastddsChannel* cmd_dto_pub_{nullptr};
    std::string cmd_dto_topic_;
    std::string cmd_dto_schema_;

    wxz::core::FastddsChannel* alert_pub_{nullptr};
    std::string alert_topic_;
    std::string alert_schema_;
    std::string dto_source_;

    ArmRespCache* resp_cache_{nullptr};
    std::uint64_t timeout_ms_{30'000};
    TraceContext* trace_ctx_{nullptr};

    std::string id_;
    std::uint64_t deadline_ms_{0};
    bool alert_sent_{false};

    bool publish_cmd(const EventDTOUtil::KvMap& kv) {
        ::EventDTO dto;
        dto.version = 1;
        dto.schema_id = cmd_dto_schema_;
        dto.topic = cmd_dto_topic_;
        dto.payload = EventDTOUtil::buildPayloadKv(kv);
        EventDTOUtil::fillMeta(dto, dto_source_);
        dto.event_id = kv.count("id") ? kv.at("id") : "";

        std::vector<std::uint8_t> buf;
        if (!wxz::dto::encode_event_dto_cdr(dto, buf)) return false;
        (void)cmd_dto_pub_->publish(buf.data(), buf.size());
        return true;
    }

    void publish_alert_once(const std::string& error_code, const std::string& message, const ArmResp* resp) {
        if (alert_sent_) return;
        if (!alert_pub_) return;

        EventDTOUtil::KvMap kv;
        kv["alert_level"] = "ERROR";
        kv["node_name"] = name();
        kv["error_code"] = error_code;
        kv["message"] = message;
        kv["op"] = "power_on_enable";
        kv["id"] = id_;
        kv["ts_ms"] = std::to_string(wxz::core::now_epoch_ms());
        fill_trace_fields(kv, trace_ctx_, id_);
        if (resp) {
            if (!resp->sdk_code.empty()) kv["sdk_code"] = resp->sdk_code;
            if (!resp->err_code.empty()) kv["arm_err_code"] = resp->err_code;
            if (!resp->err.empty()) kv["arm_err"] = resp->err;
            if (!resp->code.empty()) kv["arm_code"] = resp->code;
        }

        ::EventDTO dto;
        dto.version = 1;
        dto.schema_id = alert_schema_;
        dto.topic = alert_topic_;
        dto.payload = EventDTOUtil::buildPayloadKv(kv);
        EventDTOUtil::fillMeta(dto, dto_source_);
        dto.event_id = id_;

        std::vector<std::uint8_t> buf;
        if (wxz::dto::encode_event_dto_cdr(dto, buf)) {
            (void)alert_pub_->publish(buf.data(), buf.size());
            std::cerr << "[workstation_bt_service][INF] arm alert published code=" << error_code
                      << " op=power_on_enable node=" << name() << " id=" << id_ << "\n";
        }
        alert_sent_ = true;
    }
};

class ArmPathDownloadAction : public BT::StatefulActionNode {
public:
    ArmPathDownloadAction(const std::string& name,
                          const BT::NodeConfiguration& config,
                          wxz::core::FastddsChannel* cmd_dto_pub,
                          std::string cmd_dto_topic,
                          std::string cmd_dto_schema,
                          wxz::core::FastddsChannel* alert_pub,
                          std::string alert_topic,
                          std::string alert_schema,
                          std::string dto_source,
                          ArmRespCache* resp_cache,
                          std::uint64_t timeout_ms,
                          TraceContext* trace_ctx)
        : BT::StatefulActionNode(name, config),
          cmd_dto_pub_(cmd_dto_pub),
          cmd_dto_topic_(std::move(cmd_dto_topic)),
          cmd_dto_schema_(std::move(cmd_dto_schema)),
          alert_pub_(alert_pub),
          alert_topic_(std::move(alert_topic)),
          alert_schema_(std::move(alert_schema)),
          dto_source_(std::move(dto_source)),
          resp_cache_(resp_cache),
          timeout_ms_(timeout_ms),
          trace_ctx_(trace_ctx) {}

    static BT::PortsList providedPorts() {
        return {
            BT::InputPort<std::string>("file"),
            BT::InputPort<std::string>("index"),
            BT::InputPort<std::string>("moveType"),
            BT::InputPort<std::string>("maxPoints"),
        };
    }

    BT::NodeStatus onStart() override {
        if (!cmd_dto_pub_ || !resp_cache_) return BT::NodeStatus::FAILURE;

        id_ = make_id();
        deadline_ms_ = now_monotonic_ms() + timeout_ms_;
        alert_sent_ = false;

        EventDTOUtil::KvMap kv;
        kv["op"] = "path_download";
        kv["id"] = id_;
        fill_trace_fields(kv, trace_ctx_, id_);
        kv["file"] = getInput<std::string>("file").value_or("");
        kv["index"] = getInput<std::string>("index").value_or("1");
        kv["moveType"] = getInput<std::string>("moveType").value_or("1");
        kv["maxPoints"] = getInput<std::string>("maxPoints").value_or("10000");

        if (!publish_cmd(kv)) return BT::NodeStatus::FAILURE;
        return BT::NodeStatus::RUNNING;
    }

    BT::NodeStatus onRunning() override {
        if (now_monotonic_ms() > deadline_ms_) {
            publish_alert_once("E_ARM_TIMEOUT", "timeout waiting for /arm/status", nullptr);
            return BT::NodeStatus::FAILURE;
        }
        auto r = resp_cache_->get(id_);
        if (!r) return BT::NodeStatus::RUNNING;
        if (prefer_err_code_success(r->ok, r->err_code)) return BT::NodeStatus::SUCCESS;
        publish_alert_once("E_ARM_EXEC_FAIL", "arm command failed", &(*r));
        return BT::NodeStatus::FAILURE;
    }

    void onHalted() override {
        id_.clear();
        deadline_ms_ = 0;
    }

private:
    wxz::core::FastddsChannel* cmd_dto_pub_{nullptr};
    std::string cmd_dto_topic_;
    std::string cmd_dto_schema_;

    wxz::core::FastddsChannel* alert_pub_{nullptr};
    std::string alert_topic_;
    std::string alert_schema_;
    std::string dto_source_;

    ArmRespCache* resp_cache_{nullptr};
    std::uint64_t timeout_ms_{60'000};
    TraceContext* trace_ctx_{nullptr};

    std::string id_;
    std::uint64_t deadline_ms_{0};
    bool alert_sent_{false};

    bool publish_cmd(const EventDTOUtil::KvMap& kv) {
        ::EventDTO dto;
        dto.version = 1;
        dto.schema_id = cmd_dto_schema_;
        dto.topic = cmd_dto_topic_;
        dto.payload = EventDTOUtil::buildPayloadKv(kv);
        EventDTOUtil::fillMeta(dto, dto_source_);
        dto.event_id = kv.count("id") ? kv.at("id") : "";

        std::vector<std::uint8_t> buf;
        if (!wxz::dto::encode_event_dto_cdr(dto, buf)) return false;
        (void)cmd_dto_pub_->publish(buf.data(), buf.size());
        return true;
    }

    void publish_alert_once(const std::string& error_code, const std::string& message, const ArmResp* resp) {
        if (alert_sent_) return;
        if (!alert_pub_) return;

        EventDTOUtil::KvMap kv;
        kv["alert_level"] = "ERROR";
        kv["node_name"] = name();
        kv["error_code"] = error_code;
        kv["message"] = message;
        kv["op"] = "path_download";
        kv["id"] = id_;
        kv["ts_ms"] = std::to_string(wxz::core::now_epoch_ms());
        fill_trace_fields(kv, trace_ctx_, id_);
        if (resp) {
            if (!resp->sdk_code.empty()) kv["sdk_code"] = resp->sdk_code;
            if (!resp->err_code.empty()) kv["arm_err_code"] = resp->err_code;
            if (!resp->err.empty()) kv["arm_err"] = resp->err;
            if (!resp->code.empty()) kv["arm_code"] = resp->code;
        }

        ::EventDTO dto;
        dto.version = 1;
        dto.schema_id = alert_schema_;
        dto.topic = alert_topic_;
        dto.payload = EventDTOUtil::buildPayloadKv(kv);
        EventDTOUtil::fillMeta(dto, dto_source_);
        dto.event_id = id_;

        std::vector<std::uint8_t> buf;
        if (wxz::dto::encode_event_dto_cdr(dto, buf)) {
            (void)alert_pub_->publish(buf.data(), buf.size());
            std::cerr << "[workstation_bt_service][INF] arm alert published code=" << error_code
                      << " op=path_download node=" << name() << " id=" << id_ << "\n";
        }
        alert_sent_ = true;
    }
};

class ArmMoveJAction : public BT::StatefulActionNode {
public:
    ArmMoveJAction(const std::string& name,
                   const BT::NodeConfiguration& config,
                   wxz::core::FastddsChannel* cmd_dto_pub,
                   std::string cmd_dto_topic,
                   std::string cmd_dto_schema,
                   ArmRespCache* resp_cache,
                   std::uint64_t timeout_ms,
                   TraceContext* trace_ctx)
        : BT::StatefulActionNode(name, config),
          cmd_dto_pub_(cmd_dto_pub),
          cmd_dto_topic_(std::move(cmd_dto_topic)),
          cmd_dto_schema_(std::move(cmd_dto_schema)),
          resp_cache_(resp_cache),
          timeout_ms_(timeout_ms),
          trace_ctx_(trace_ctx) {}

    static BT::PortsList providedPorts() {
        return {
            BT::InputPort<std::string>("jointpos"),
            BT::InputPort<std::string>("speed"),
        };
    }

    BT::NodeStatus onStart() override {
        if (!cmd_dto_pub_ || !resp_cache_) return BT::NodeStatus::FAILURE;

        id_ = make_id();
        deadline_ms_ = now_monotonic_ms() + timeout_ms_;

        EventDTOUtil::KvMap kv;
        kv["op"] = "moveJoint";
        kv["id"] = id_;
        fill_trace_fields(kv, trace_ctx_, id_);
        kv["jointpos"] = getInput<std::string>("jointpos").value_or("");
        kv["speed"] = getInput<std::string>("speed").value_or("3.14");

        if (!publish_cmd(kv)) return BT::NodeStatus::FAILURE;
        return BT::NodeStatus::RUNNING;
    }

    BT::NodeStatus onRunning() override {
        if (now_monotonic_ms() > deadline_ms_) return BT::NodeStatus::FAILURE;
        auto r = resp_cache_->get(id_);
        if (!r) return BT::NodeStatus::RUNNING;
        return prefer_err_code_success(r->ok, r->err_code) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    }

    void onHalted() override {
        id_.clear();
        deadline_ms_ = 0;
    }

private:
    wxz::core::FastddsChannel* cmd_dto_pub_{nullptr};
    std::string cmd_dto_topic_;
    std::string cmd_dto_schema_;

    ArmRespCache* resp_cache_{nullptr};
    std::uint64_t timeout_ms_{30'000};
    TraceContext* trace_ctx_{nullptr};

    std::string id_;
    std::uint64_t deadline_ms_{0};

    bool publish_cmd(const EventDTOUtil::KvMap& kv) {
        ::EventDTO dto;
        dto.version = 1;
        dto.schema_id = cmd_dto_schema_;
        dto.topic = cmd_dto_topic_;
        dto.payload = EventDTOUtil::buildPayloadKv(kv);
        EventDTOUtil::fillMeta(dto, "workstation_bt_service");
        dto.event_id = kv.count("id") ? kv.at("id") : "";

        std::vector<std::uint8_t> buf;
        if (!wxz::dto::encode_event_dto_cdr(dto, buf)) return false;
        (void)cmd_dto_pub_->publish(buf.data(), buf.size());
        return true;
    }
};

class ArmSimpleOpAction : public BT::StatefulActionNode {
public:
    ArmSimpleOpAction(const std::string& name,
                      const BT::NodeConfiguration& config,
                      std::string op,
                      wxz::core::FastddsChannel* cmd_dto_pub,
                      std::string cmd_dto_topic,
                      std::string cmd_dto_schema,
                      ArmRespCache* resp_cache,
                      std::uint64_t timeout_ms,
                      TraceContext* trace_ctx)
        : BT::StatefulActionNode(name, config),
          op_(std::move(op)),
          cmd_dto_pub_(cmd_dto_pub),
          cmd_dto_topic_(std::move(cmd_dto_topic)),
          cmd_dto_schema_(std::move(cmd_dto_schema)),
          resp_cache_(resp_cache),
          timeout_ms_(timeout_ms),
          trace_ctx_(trace_ctx) {}

    static BT::PortsList providedPorts() {
        return {
            BT::InputPort<std::string>("enable"),
            BT::InputPort<std::string>("timeout_ms"),
        };
    }

    BT::NodeStatus onStart() override {
        if (!cmd_dto_pub_ || !resp_cache_) return BT::NodeStatus::FAILURE;

        id_ = make_id();
        const std::uint64_t timeout = timeout_ms_override_or_default();
        deadline_ms_ = now_monotonic_ms() + timeout;

        EventDTOUtil::KvMap kv;
        kv["op"] = op_;
        kv["id"] = id_;
        fill_trace_fields(kv, trace_ctx_, id_);
        if (const auto enable = getInput<std::string>("enable")) {
            kv["enable"] = enable.value();
        }
        if (const auto t = getInput<std::string>("timeout_ms")) {
            if (!t->empty()) kv["timeout_ms"] = *t;
        }

        if (!publish_cmd(kv)) return BT::NodeStatus::FAILURE;
        return BT::NodeStatus::RUNNING;
    }

    BT::NodeStatus onRunning() override {
        if (now_monotonic_ms() > deadline_ms_) return BT::NodeStatus::FAILURE;
        auto r = resp_cache_->get(id_);
        if (!r) return BT::NodeStatus::RUNNING;
        return prefer_err_code_success(r->ok, r->err_code) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    }

    void onHalted() override {
        id_.clear();
        deadline_ms_ = 0;
    }

private:
    std::string op_;
    wxz::core::FastddsChannel* cmd_dto_pub_{nullptr};
    std::string cmd_dto_topic_;
    std::string cmd_dto_schema_;

    ArmRespCache* resp_cache_{nullptr};
    std::uint64_t timeout_ms_{30'000};
    TraceContext* trace_ctx_{nullptr};

    std::string id_;
    std::uint64_t deadline_ms_{0};

    std::uint64_t timeout_ms_override_or_default() const {
        const auto t = getInput<std::string>("timeout_ms");
        if (!t || t->empty()) return timeout_ms_;
        try {
            return static_cast<std::uint64_t>(std::stoull(*t));
        } catch (...) {
            return timeout_ms_;
        }
    }

    bool publish_cmd(const EventDTOUtil::KvMap& kv) {
        ::EventDTO dto;
        dto.version = 1;
        dto.schema_id = cmd_dto_schema_;
        dto.topic = cmd_dto_topic_;
        dto.payload = EventDTOUtil::buildPayloadKv(kv);
        EventDTOUtil::fillMeta(dto, "workstation_bt_service");
        dto.event_id = kv.count("id") ? kv.at("id") : "";

        std::vector<std::uint8_t> buf;
        if (!wxz::dto::encode_event_dto_cdr(dto, buf)) return false;
        (void)cmd_dto_pub_->publish(buf.data(), buf.size());
        return true;
    }
};

class ArmBoolQueryAction : public BT::StatefulActionNode {
public:
    ArmBoolQueryAction(const std::string& name,
                       const BT::NodeConfiguration& config,
                       std::string op,
                       wxz::core::FastddsChannel* cmd_dto_pub,
                       std::string cmd_dto_topic,
                       std::string cmd_dto_schema,
                       ArmRespCache* resp_cache,
                       std::uint64_t timeout_ms,
                       TraceContext* trace_ctx)
        : BT::StatefulActionNode(name, config),
          op_(std::move(op)),
          cmd_dto_pub_(cmd_dto_pub),
          cmd_dto_topic_(std::move(cmd_dto_topic)),
          cmd_dto_schema_(std::move(cmd_dto_schema)),
          resp_cache_(resp_cache),
          timeout_ms_(timeout_ms),
          trace_ctx_(trace_ctx) {}

    static BT::PortsList providedPorts() {
        return {
            BT::InputPort<std::string>("timeout_ms"),
        };
    }

    BT::NodeStatus onStart() override {
        if (!cmd_dto_pub_ || !resp_cache_) return BT::NodeStatus::FAILURE;

        id_ = make_id();
        const std::uint64_t timeout = timeout_ms_override_or_default();
        deadline_ms_ = now_monotonic_ms() + timeout;

        EventDTOUtil::KvMap kv;
        kv["op"] = op_;
        kv["id"] = id_;
        fill_trace_fields(kv, trace_ctx_, id_);
        if (const auto t = getInput<std::string>("timeout_ms")) {
            if (!t->empty()) kv["timeout_ms"] = *t;
        }

        if (!publish_cmd(kv)) return BT::NodeStatus::FAILURE;
        return BT::NodeStatus::RUNNING;
    }

    BT::NodeStatus onRunning() override {
        if (now_monotonic_ms() > deadline_ms_) return BT::NodeStatus::FAILURE;
        auto r = resp_cache_->get(id_);
        if (!r) return BT::NodeStatus::RUNNING;
        if (!prefer_err_code_success(r->ok, r->err_code)) return BT::NodeStatus::FAILURE;
        const std::string v = kv_get_or(r->kv, "value", "0");
        return is_truthy(v) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    }

    void onHalted() override {
        id_.clear();
        deadline_ms_ = 0;
    }

private:
    std::string op_;
    wxz::core::FastddsChannel* cmd_dto_pub_{nullptr};
    std::string cmd_dto_topic_;
    std::string cmd_dto_schema_;

    ArmRespCache* resp_cache_{nullptr};
    std::uint64_t timeout_ms_{10'000};
    TraceContext* trace_ctx_{nullptr};

    std::string id_;
    std::uint64_t deadline_ms_{0};

    std::uint64_t timeout_ms_override_or_default() const {
        const auto t = getInput<std::string>("timeout_ms");
        if (!t || t->empty()) return timeout_ms_;
        try {
            return static_cast<std::uint64_t>(std::stoull(*t));
        } catch (...) {
            return timeout_ms_;
        }
    }

    bool publish_cmd(const EventDTOUtil::KvMap& kv) {
        ::EventDTO dto;
        dto.version = 1;
        dto.schema_id = cmd_dto_schema_;
        dto.topic = cmd_dto_topic_;
        dto.payload = EventDTOUtil::buildPayloadKv(kv);
        EventDTOUtil::fillMeta(dto, "workstation_bt_service");
        dto.event_id = kv.count("id") ? kv.at("id") : "";

        std::vector<std::uint8_t> buf;
        if (!wxz::dto::encode_event_dto_cdr(dto, buf)) return false;
        (void)cmd_dto_pub_->publish(buf.data(), buf.size());
        return true;
    }
};

class ArmGetRobotModeAction : public BT::StatefulActionNode {
public:
    ArmGetRobotModeAction(const std::string& name,
                          const BT::NodeConfiguration& config,
                          wxz::core::FastddsChannel* cmd_dto_pub,
                          std::string cmd_dto_topic,
                          std::string cmd_dto_schema,
                          ArmRespCache* resp_cache,
                          std::uint64_t timeout_ms,
                          TraceContext* trace_ctx)
        : BT::StatefulActionNode(name, config),
          cmd_dto_pub_(cmd_dto_pub),
          cmd_dto_topic_(std::move(cmd_dto_topic)),
          cmd_dto_schema_(std::move(cmd_dto_schema)),
          resp_cache_(resp_cache),
          timeout_ms_(timeout_ms),
          trace_ctx_(trace_ctx) {}

    static BT::PortsList providedPorts() {
        return {
            BT::OutputPort<std::string>("mode"),
            BT::InputPort<std::string>("timeout_ms"),
        };
    }

    BT::NodeStatus onStart() override {
        if (!cmd_dto_pub_ || !resp_cache_) return BT::NodeStatus::FAILURE;

        id_ = make_id();
        const std::uint64_t timeout = timeout_ms_override_or_default();
        deadline_ms_ = now_monotonic_ms() + timeout;

        EventDTOUtil::KvMap kv;
        kv["op"] = "robot_mode";
        kv["id"] = id_;
        fill_trace_fields(kv, trace_ctx_, id_);

        if (!publish_cmd(kv)) return BT::NodeStatus::FAILURE;
        return BT::NodeStatus::RUNNING;
    }

    BT::NodeStatus onRunning() override {
        if (now_monotonic_ms() > deadline_ms_) return BT::NodeStatus::FAILURE;
        auto r = resp_cache_->get(id_);
        if (!r) return BT::NodeStatus::RUNNING;
        if (!prefer_err_code_success(r->ok, r->err_code)) return BT::NodeStatus::FAILURE;
        const std::string mode = kv_get_or(r->kv, "mode", "");
        (void)setOutput("mode", mode);
        return BT::NodeStatus::SUCCESS;
    }

    void onHalted() override {
        id_.clear();
        deadline_ms_ = 0;
    }

private:
    wxz::core::FastddsChannel* cmd_dto_pub_{nullptr};
    std::string cmd_dto_topic_;
    std::string cmd_dto_schema_;

    ArmRespCache* resp_cache_{nullptr};
    std::uint64_t timeout_ms_{5'000};
    TraceContext* trace_ctx_{nullptr};

    std::string id_;
    std::uint64_t deadline_ms_{0};

    std::uint64_t timeout_ms_override_or_default() const {
        const auto t = getInput<std::string>("timeout_ms");
        if (!t || t->empty()) return timeout_ms_;
        try {
            return static_cast<std::uint64_t>(std::stoull(*t));
        } catch (...) {
            return timeout_ms_;
        }
    }

    bool publish_cmd(const EventDTOUtil::KvMap& kv) {
        ::EventDTO dto;
        dto.version = 1;
        dto.schema_id = cmd_dto_schema_;
        dto.topic = cmd_dto_topic_;
        dto.payload = EventDTOUtil::buildPayloadKv(kv);
        EventDTOUtil::fillMeta(dto, "workstation_bt_service");
        dto.event_id = kv.count("id") ? kv.at("id") : "";

        std::vector<std::uint8_t> buf;
        if (!wxz::dto::encode_event_dto_cdr(dto, buf)) return false;
        (void)cmd_dto_pub_->publish(buf.data(), buf.size());
        return true;
    }
};

class ArmGetJointActualPosAction : public BT::StatefulActionNode {
public:
    ArmGetJointActualPosAction(const std::string& name,
                               const BT::NodeConfiguration& config,
                               wxz::core::FastddsChannel* cmd_dto_pub,
                               std::string cmd_dto_topic,
                               std::string cmd_dto_schema,
                               ArmRespCache* resp_cache,
                               std::uint64_t timeout_ms,
                               TraceContext* trace_ctx)
        : BT::StatefulActionNode(name, config),
          cmd_dto_pub_(cmd_dto_pub),
          cmd_dto_topic_(std::move(cmd_dto_topic)),
          cmd_dto_schema_(std::move(cmd_dto_schema)),
          resp_cache_(resp_cache),
          timeout_ms_(timeout_ms),
          trace_ctx_(trace_ctx) {}

    static BT::PortsList providedPorts() {
        return {
            BT::OutputPort<std::string>("jointpos"),
            BT::InputPort<std::string>("timeout_ms"),
        };
    }

    BT::NodeStatus onStart() override {
        if (!cmd_dto_pub_ || !resp_cache_) return BT::NodeStatus::FAILURE;

        id_ = make_id();
        const std::uint64_t timeout = timeout_ms_override_or_default();
        deadline_ms_ = now_monotonic_ms() + timeout;

        EventDTOUtil::KvMap kv;
        kv["op"] = "get_joint_actual_pos";
        kv["id"] = id_;
        fill_trace_fields(kv, trace_ctx_, id_);

        if (!publish_cmd(kv)) return BT::NodeStatus::FAILURE;
        return BT::NodeStatus::RUNNING;
    }

    BT::NodeStatus onRunning() override {
        if (now_monotonic_ms() > deadline_ms_) return BT::NodeStatus::FAILURE;
        auto r = resp_cache_->get(id_);
        if (!r) return BT::NodeStatus::RUNNING;
        if (!prefer_err_code_success(r->ok, r->err_code)) return BT::NodeStatus::FAILURE;

        const std::string jointpos = kv_get_or(r->kv, "jointpos", "");
        if (jointpos.empty()) return BT::NodeStatus::FAILURE;
        (void)setOutput("jointpos", jointpos);
        const std::string jointpos_deg = kv_get_or(r->kv, "jointpos_deg", "");
        std::cerr << "[workstation_bt_service][INF] get_joint_actual_pos jointpos(rad)=" << jointpos;
        if (!jointpos_deg.empty()) std::cerr << " jointpos_deg=" << jointpos_deg;
        std::cerr << "\n";
        return BT::NodeStatus::SUCCESS;
    }

    void onHalted() override {
        id_.clear();
        deadline_ms_ = 0;
    }

private:
    wxz::core::FastddsChannel* cmd_dto_pub_{nullptr};
    std::string cmd_dto_topic_;
    std::string cmd_dto_schema_;

    ArmRespCache* resp_cache_{nullptr};
    std::uint64_t timeout_ms_{5'000};
    TraceContext* trace_ctx_{nullptr};

    std::string id_;
    std::uint64_t deadline_ms_{0};

    std::uint64_t timeout_ms_override_or_default() const {
        const auto t = getInput<std::string>("timeout_ms");
        if (!t || t->empty()) return timeout_ms_;
        try {
            return static_cast<std::uint64_t>(std::stoull(*t));
        } catch (...) {
            return timeout_ms_;
        }
    }

    bool publish_cmd(const EventDTOUtil::KvMap& kv) {
        ::EventDTO dto;
        dto.version = 1;
        dto.schema_id = cmd_dto_schema_;
        dto.topic = cmd_dto_topic_;
        dto.payload = EventDTOUtil::buildPayloadKv(kv);
        EventDTOUtil::fillMeta(dto, "workstation_bt_service");
        dto.event_id = kv.count("id") ? kv.at("id") : "";

        std::vector<std::uint8_t> buf;
        if (!wxz::dto::encode_event_dto_cdr(dto, buf)) return false;
        (void)cmd_dto_pub_->publish(buf.data(), buf.size());
        return true;
    }
};

}  // namespace

void register_arm_control_nodes(BT::BehaviorTreeFactory& factory, ArmNodeDeps deps) {
    auto deps_sp = std::make_shared<ArmNodeDeps>(std::move(deps));
    const std::uint64_t timeout_ms = deps_sp->arm_timeout_ms;

    factory.registerBuilder<ArmPowerOnAction>(
        "ArmPowerOn",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmPowerOnAction>(name,
                                                      config,
                                                      deps_sp->arm_cmd_dto_pub,
                                                      deps_sp->arm_cmd_dto_topic,
                                                      deps_sp->arm_cmd_dto_schema,
                                                      deps_sp->system_alert_dto_pub,
                                                      deps_sp->system_alert_dto_topic,
                                                      deps_sp->system_alert_dto_schema,
                                                      deps_sp->dto_source,
                                                      deps_sp->arm_cache,
                                                      timeout_ms,
                                                      deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmPowerOnAction>(
        "PowerOn",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmPowerOnAction>(name,
                                                      config,
                                                      deps_sp->arm_cmd_dto_pub,
                                                      deps_sp->arm_cmd_dto_topic,
                                                      deps_sp->arm_cmd_dto_schema,
                                                      deps_sp->system_alert_dto_pub,
                                                      deps_sp->system_alert_dto_topic,
                                                      deps_sp->system_alert_dto_schema,
                                                      deps_sp->dto_source,
                                                      deps_sp->arm_cache,
                                                      timeout_ms,
                                                      deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmPowerOnAction>(
        "power_on_enable",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmPowerOnAction>(name,
                                                      config,
                                                      deps_sp->arm_cmd_dto_pub,
                                                      deps_sp->arm_cmd_dto_topic,
                                                      deps_sp->arm_cmd_dto_schema,
                                                      deps_sp->system_alert_dto_pub,
                                                      deps_sp->system_alert_dto_topic,
                                                      deps_sp->system_alert_dto_schema,
                                                      deps_sp->dto_source,
                                                      deps_sp->arm_cache,
                                                      timeout_ms,
                                                      deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmPowerOnAction>(
        "InitializeArm",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmPowerOnAction>(name,
                                                      config,
                                                      deps_sp->arm_cmd_dto_pub,
                                                      deps_sp->arm_cmd_dto_topic,
                                                      deps_sp->arm_cmd_dto_schema,
                                                      deps_sp->system_alert_dto_pub,
                                                      deps_sp->system_alert_dto_topic,
                                                      deps_sp->system_alert_dto_schema,
                                                      deps_sp->dto_source,
                                                      deps_sp->arm_cache,
                                                      timeout_ms,
                                                      deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmMoveLAction>(
        "ArmMoveL",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmMoveLAction>(name,
                                                    config,
                                                    deps_sp->arm_cmd_dto_pub,
                                                    deps_sp->arm_cmd_dto_topic,
                                                    deps_sp->arm_cmd_dto_schema,
                                                    deps_sp->system_alert_dto_pub,
                                                    deps_sp->system_alert_dto_topic,
                                                    deps_sp->system_alert_dto_schema,
                                                    deps_sp->dto_source,
                                                    deps_sp->arm_cache,
                                                    timeout_ms,
                                                    deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmMoveLAction>(
        "MoveL",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmMoveLAction>(name,
                                                    config,
                                                    deps_sp->arm_cmd_dto_pub,
                                                    deps_sp->arm_cmd_dto_topic,
                                                    deps_sp->arm_cmd_dto_schema,
                                                    deps_sp->system_alert_dto_pub,
                                                    deps_sp->system_alert_dto_topic,
                                                    deps_sp->system_alert_dto_schema,
                                                    deps_sp->dto_source,
                                                    deps_sp->arm_cache,
                                                    timeout_ms,
                                                    deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmMoveLAction>(
        "moveL",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmMoveLAction>(name,
                                                    config,
                                                    deps_sp->arm_cmd_dto_pub,
                                                    deps_sp->arm_cmd_dto_topic,
                                                    deps_sp->arm_cmd_dto_schema,
                                                    deps_sp->system_alert_dto_pub,
                                                    deps_sp->system_alert_dto_topic,
                                                    deps_sp->system_alert_dto_schema,
                                                    deps_sp->dto_source,
                                                    deps_sp->arm_cache,
                                                    timeout_ms,
                                                    deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmMoveJAction>(
        "ArmMoveJ",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmMoveJAction>(name,
                                                    config,
                                                    deps_sp->arm_cmd_dto_pub,
                                                    deps_sp->arm_cmd_dto_topic,
                                                    deps_sp->arm_cmd_dto_schema,
                                                    deps_sp->arm_cache,
                                                    timeout_ms,
                                                    deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmMoveJAction>(
        "MoveJ",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmMoveJAction>(name,
                                                    config,
                                                    deps_sp->arm_cmd_dto_pub,
                                                    deps_sp->arm_cmd_dto_topic,
                                                    deps_sp->arm_cmd_dto_schema,
                                                    deps_sp->arm_cache,
                                                    timeout_ms,
                                                    deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmMoveJAction>(
        "moveJ",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmMoveJAction>(name,
                                                    config,
                                                    deps_sp->arm_cmd_dto_pub,
                                                    deps_sp->arm_cmd_dto_topic,
                                                    deps_sp->arm_cmd_dto_schema,
                                                    deps_sp->arm_cache,
                                                    timeout_ms,
                                                    deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmMoveJAction>(
        "moveJoint",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmMoveJAction>(name,
                                                    config,
                                                    deps_sp->arm_cmd_dto_pub,
                                                    deps_sp->arm_cmd_dto_topic,
                                                    deps_sp->arm_cmd_dto_schema,
                                                    deps_sp->arm_cache,
                                                    timeout_ms,
                                                    deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmPathDownloadAction>(
        "ArmPathDownload",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmPathDownloadAction>(name,
                                                           config,
                                                           deps_sp->arm_cmd_dto_pub,
                                                           deps_sp->arm_cmd_dto_topic,
                                                           deps_sp->arm_cmd_dto_schema,
                                                           deps_sp->system_alert_dto_pub,
                                                           deps_sp->system_alert_dto_topic,
                                                           deps_sp->system_alert_dto_schema,
                                                           deps_sp->dto_source,
                                                           deps_sp->arm_cache,
                                                           timeout_ms,
                                                           deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmPathDownloadAction>(
        "path_download",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmPathDownloadAction>(name,
                                                           config,
                                                           deps_sp->arm_cmd_dto_pub,
                                                           deps_sp->arm_cmd_dto_topic,
                                                           deps_sp->arm_cmd_dto_schema,
                                                           deps_sp->system_alert_dto_pub,
                                                           deps_sp->system_alert_dto_topic,
                                                           deps_sp->system_alert_dto_schema,
                                                           deps_sp->dto_source,
                                                           deps_sp->arm_cache,
                                                           timeout_ms,
                                                           deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmSimpleOpAction>(
        "fault_reset",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmSimpleOpAction>(name,
                                                       config,
                                                       "fault_reset",
                                                       deps_sp->arm_cmd_dto_pub,
                                                       deps_sp->arm_cmd_dto_topic,
                                                       deps_sp->arm_cmd_dto_schema,
                                                       deps_sp->arm_cache,
                                                       timeout_ms,
                                                       deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmSimpleOpAction>(
        "FaultReset",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmSimpleOpAction>(name,
                                                       config,
                                                       "fault_reset",
                                                       deps_sp->arm_cmd_dto_pub,
                                                       deps_sp->arm_cmd_dto_topic,
                                                       deps_sp->arm_cmd_dto_schema,
                                                       deps_sp->arm_cache,
                                                       timeout_ms,
                                                       deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmSimpleOpAction>(
        "reset_system",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmSimpleOpAction>(name,
                                                       config,
                                                       "reset_system",
                                                       deps_sp->arm_cmd_dto_pub,
                                                       deps_sp->arm_cmd_dto_topic,
                                                       deps_sp->arm_cmd_dto_schema,
                                                       deps_sp->arm_cache,
                                                       timeout_ms,
                                                       deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmSimpleOpAction>(
        "ResetSystem",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmSimpleOpAction>(name,
                                                       config,
                                                       "reset_system",
                                                       deps_sp->arm_cmd_dto_pub,
                                                       deps_sp->arm_cmd_dto_topic,
                                                       deps_sp->arm_cmd_dto_schema,
                                                       deps_sp->arm_cache,
                                                       timeout_ms,
                                                       deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmSimpleOpAction>(
        "slow_speed",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmSimpleOpAction>(name,
                                                       config,
                                                       "slow_speed",
                                                       deps_sp->arm_cmd_dto_pub,
                                                       deps_sp->arm_cmd_dto_topic,
                                                       deps_sp->arm_cmd_dto_schema,
                                                       deps_sp->arm_cache,
                                                       timeout_ms,
                                                       deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmSimpleOpAction>(
        "slowSpeed",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmSimpleOpAction>(name,
                                                       config,
                                                       "slowSpeed",
                                                       deps_sp->arm_cmd_dto_pub,
                                                       deps_sp->arm_cmd_dto_topic,
                                                       deps_sp->arm_cmd_dto_schema,
                                                       deps_sp->arm_cache,
                                                       timeout_ms,
                                                       deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmSimpleOpAction>(
        "quick_stop",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmSimpleOpAction>(name,
                                                       config,
                                                       "quick_stop",
                                                       deps_sp->arm_cmd_dto_pub,
                                                       deps_sp->arm_cmd_dto_topic,
                                                       deps_sp->arm_cmd_dto_schema,
                                                       deps_sp->arm_cache,
                                                       timeout_ms,
                                                       deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmSimpleOpAction>(
        "quickStop",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmSimpleOpAction>(name,
                                                       config,
                                                       "quickStop",
                                                       deps_sp->arm_cmd_dto_pub,
                                                       deps_sp->arm_cmd_dto_topic,
                                                       deps_sp->arm_cmd_dto_schema,
                                                       deps_sp->arm_cache,
                                                       timeout_ms,
                                                       deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmSimpleOpAction>(
        "emergency_stop",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmSimpleOpAction>(name,
                                                       config,
                                                       "emergency_stop",
                                                       deps_sp->arm_cmd_dto_pub,
                                                       deps_sp->arm_cmd_dto_topic,
                                                       deps_sp->arm_cmd_dto_schema,
                                                       deps_sp->arm_cache,
                                                       timeout_ms,
                                                       deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmSimpleOpAction>(
        "EmergencyStop",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmSimpleOpAction>(name,
                                                       config,
                                                       "emergency_stop",
                                                       deps_sp->arm_cmd_dto_pub,
                                                       deps_sp->arm_cmd_dto_topic,
                                                       deps_sp->arm_cmd_dto_schema,
                                                       deps_sp->arm_cache,
                                                       timeout_ms,
                                                       deps_sp->trace_ctx);
        });

    auto reg_bool = [&factory, deps_sp, timeout_ms](const char* name, const char* op) {
        const std::string op_s(op);
        factory.registerBuilder<ArmBoolQueryAction>(
            name,
            [deps_sp, timeout_ms, op_s](const std::string& n, const BT::NodeConfiguration& config) {
                return std::make_unique<ArmBoolQueryAction>(n,
                                                            config,
                                                            op_s,
                                                            deps_sp->arm_cmd_dto_pub,
                                                            deps_sp->arm_cmd_dto_topic,
                                                            deps_sp->arm_cmd_dto_schema,
                                                            deps_sp->arm_cache,
                                                            timeout_ms,
                                                            deps_sp->trace_ctx);
            });
    };

    reg_bool("IsArmReady", "is_arm_ready");
    reg_bool("IsPowerOn", "is_power_on");
    reg_bool("IsStartSignal", "is_start_signal");
    reg_bool("IsStopSignal", "is_stop_signal");
    reg_bool("IsTrajectoryComplete", "is_trajectory_complete");
    reg_bool("IsAllTrajectoriesComplete", "is_all_trajectories_complete");

    reg_bool("wait_for_start", "wait_for_start");
    reg_bool("WaitForStart", "wait_for_start");

    reg_bool("execute_trajectory", "execute_trajectory");
    reg_bool("ExecuteTrajectory", "execute_trajectory");

    factory.registerBuilder<ArmGetRobotModeAction>(
        "get_robot_mode",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmGetRobotModeAction>(name,
                                                           config,
                                                           deps_sp->arm_cmd_dto_pub,
                                                           deps_sp->arm_cmd_dto_topic,
                                                           deps_sp->arm_cmd_dto_schema,
                                                           deps_sp->arm_cache,
                                                           timeout_ms,
                                                           deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmGetRobotModeAction>(
        "GetRobotMode",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmGetRobotModeAction>(name,
                                                           config,
                                                           deps_sp->arm_cmd_dto_pub,
                                                           deps_sp->arm_cmd_dto_topic,
                                                           deps_sp->arm_cmd_dto_schema,
                                                           deps_sp->arm_cache,
                                                           timeout_ms,
                                                           deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmGetJointActualPosAction>(
        "get_joint_actual_pos",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmGetJointActualPosAction>(name,
                                                                config,
                                                                deps_sp->arm_cmd_dto_pub,
                                                                deps_sp->arm_cmd_dto_topic,
                                                                deps_sp->arm_cmd_dto_schema,
                                                                deps_sp->arm_cache,
                                                                timeout_ms,
                                                                deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmGetJointActualPosAction>(
        "GetJointActualPos",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmGetJointActualPosAction>(name,
                                                                config,
                                                                deps_sp->arm_cmd_dto_pub,
                                                                deps_sp->arm_cmd_dto_topic,
                                                                deps_sp->arm_cmd_dto_schema,
                                                                deps_sp->arm_cache,
                                                                timeout_ms,
                                                                deps_sp->trace_ctx);
        });

    factory.registerBuilder<ArmGetJointActualPosAction>(
        "ArmGetJointActualPos",
        [deps_sp, timeout_ms](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<ArmGetJointActualPosAction>(name,
                                                                config,
                                                                deps_sp->arm_cmd_dto_pub,
                                                                deps_sp->arm_cmd_dto_topic,
                                                                deps_sp->arm_cmd_dto_schema,
                                                                deps_sp->arm_cache,
                                                                timeout_ms,
                                                                deps_sp->trace_ctx);
        });
}

}  // namespace wxz::workstation::bt_service
