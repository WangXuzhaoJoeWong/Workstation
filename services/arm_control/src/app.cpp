#include "app.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>

#include "fault_recovery_executor.h"
#include "metrics_http_server.h"
#include "metrics_prometheus.h"

#include "internal/arm_command_handler.h"
#include "internal/arm_control_config.h"
#include "internal/arm_control_internal.h"
#include "internal/arm_error_codes.h"
#include "internal/arm_command_processor.h"
#include "internal/arm_control_loop.h"

#include "executor.h"
#include "fastdds_channel.h"
#include "strand.h"

#include "internal/rpc_control_plane.h"

#include "workstation/node.h"

#include "node_base.h"
#include "service_common.h"
#include "logger.h"

namespace wxz::workstation::arm_control {

namespace {

struct MetricsExportRuntime {
    wxz::core::PrometheusMetricsSink sink;
    std::unique_ptr<wxz::core::MetricsHttpServer> http;
    std::thread worker;

    ~MetricsExportRuntime() {
        if (worker.joinable()) worker.join();
    }
};

static std::unique_ptr<MetricsExportRuntime> maybe_start_metrics_export(wxz::core::NodeBase& node,
                                                                        wxz::core::Logger& logger,
                                                                        std::string service) {
    const int enable = wxz::core::getenv_int("WXZ_METRICS_EXPORT_ENABLE", 0);
    if (!enable) return nullptr;

    const int period_ms = wxz::core::getenv_int("WXZ_METRICS_EXPORT_PERIOD_MS",
                                                wxz::core::getenv_int("WXZ_METRICS_PERIOD_MS", 5000));
    const std::string path = wxz::core::getenv_str("WXZ_METRICS_EXPORT_PATH", "");
    if (period_ms <= 0) return nullptr;

    auto rt = std::make_unique<MetricsExportRuntime>();
    wxz::core::set_metrics_sink(&rt->sink);

    // 可选：启用 HTTP /metrics 端点
    {
        const int http_enable = wxz::core::getenv_int("WXZ_METRICS_HTTP_ENABLE", 0);
        if (http_enable) {
            wxz::core::MetricsHttpServer::Options o;
            o.bind_addr = wxz::core::getenv_str("WXZ_METRICS_HTTP_ADDR", "0.0.0.0");
            o.port = wxz::core::getenv_int("WXZ_METRICS_HTTP_PORT", 9101);
            o.path = wxz::core::getenv_str("WXZ_METRICS_HTTP_PATH", "/metrics");
            rt->http = std::make_unique<wxz::core::MetricsHttpServer>(
                o,
                [sink = &rt->sink]() { return sink->render(); });
            if (!rt->http->start()) {
                logger.log(wxz::core::LogLevel::Warn,
                           "metrics_http start failed addr='" + o.bind_addr + "' port=" + std::to_string(o.port));
                rt->http.reset();
            } else {
                logger.log(wxz::core::LogLevel::Info,
                           "metrics_http enabled http://" + o.bind_addr + ":" + std::to_string(o.port) + o.path);
            }
        }
    }

    rt->worker = std::thread([&node, &logger, service = std::move(service), path, period_ms, sink = &rt->sink]() {
        using namespace std::chrono;
        while (node.running()) {
            std::this_thread::sleep_for(milliseconds(period_ms));
            if (!node.running()) break;

            const std::string text = sink->render();
            if (path.empty()) {
                logger.log(wxz::core::LogLevel::Info, "metrics_export service='" + service + "'\n" + text);
            } else {
                std::ofstream ofs(path, std::ios::trunc);
                if (!ofs) {
                    logger.log(wxz::core::LogLevel::Warn,
                               "metrics_export write failed service='" + service + "' path='" + path + "'");
                    continue;
                }
                ofs << text;
            }
        }
    });

    logger.log(wxz::core::LogLevel::Info,
               "metrics_export enabled period_ms=" + std::to_string(period_ms) +
                   (path.empty() ? " to=logger" : (" to_file='" + path + "'")));
    return rt;
}

static std::unique_ptr<wxz::core::FaultRecoveryExecutor> maybe_start_fault_recovery(wxz::core::Executor& exec,
                                                                                    wxz::core::NodeBase& node,
                                                                                    wxz::core::Logger& logger,
                                                                                    int domain,
                                                                                    const std::string& fault_topic,
                                                                                    const std::string& service,
                                                                                    std::atomic<int>& exit_code) {
    const int enable = wxz::core::getenv_int("WXZ_FAULT_RECOVERY_ENABLE", 0);
    if (!enable) return nullptr;

    const std::string action = wxz::core::getenv_str("WXZ_FAULT_RECOVERY_ACTION", "restart");
    const int default_exit = wxz::core::getenv_int("WXZ_FAULT_RECOVERY_EXIT_CODE", 77);
    const std::string marker_file = wxz::core::getenv_str("WXZ_FAULT_RECOVERY_MARKER_FILE", "/tmp/wxz_degraded");

    const std::string match_fault = wxz::core::getenv_str("WXZ_FAULT_RECOVERY_MATCH_FAULT", "");
    const std::string match_service = wxz::core::getenv_str("WXZ_FAULT_RECOVERY_MATCH_SERVICE", service);
    const std::string severities = wxz::core::getenv_str("WXZ_FAULT_RECOVERY_SEVERITY", "fatal");

    std::vector<wxz::core::FaultRecoveryRule> rules;
    {
        std::string cur;
        auto flush = [&] {
            auto s = cur;
            cur.clear();
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
            if (s.empty()) return;

            wxz::core::FaultRecoveryRule r;
            r.fault = match_fault;
            r.service = match_service;
            r.severity = s;
            r.action = action;
            r.exit_code = (default_exit == 0) ? 77 : default_exit;
            r.marker_file = marker_file;
            rules.push_back(std::move(r));
        };
        for (char c : severities) {
            if (c == ',') {
                flush();
            } else {
                cur.push_back(c);
            }
        }
        flush();
    }
    if (rules.empty()) {
        wxz::core::FaultRecoveryRule r;
        r.fault = match_fault;
        r.service = match_service;
        r.severity = "fatal";
        r.action = action;
        r.exit_code = (default_exit == 0) ? 77 : default_exit;
        r.marker_file = marker_file;
        rules.push_back(std::move(r));
    }

    auto request_restart = [&](int code) {
        const int c = (code == 0) ? 77 : code;
        exit_code.store(c);
        logger.log(wxz::core::LogLevel::Warn, "fault_recovery requested restart exit_code=" + std::to_string(c));
        node.request_stop();
    };
    auto warn = [&](const std::string& msg) { logger.log(wxz::core::LogLevel::Warn, msg); };

    auto fr = std::make_unique<wxz::core::FaultRecoveryExecutor>(domain,
                                                                 fault_topic,
                                                                 std::move(rules),
                                                                 std::move(request_restart),
                                                                 std::move(warn));
    fr->start_on(exec);

    logger.log(wxz::core::LogLevel::Info,
               "fault_recovery enabled action='" + action + "' match_service='" + match_service +
                   "' topic='" + fault_topic + "'");
    return fr;
}

} // namespace

int run() {
    using namespace wxz::workstation::arm_control::internal;

    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);

    const ArmControlConfig cfg = load_arm_control_config_from_env();

    const auto& conn = cfg.conn;
    const int domain = cfg.domain;
    const std::size_t queue_max = cfg.queue_max;
    const std::string& sw_version = cfg.sw_version;
    auto& logger = wxz::core::Logger::getInstance();
    logger.set_level(cfg.log_level);
    logger.set_prefix("[workstation_arm_control_service] ");

    std::atomic<int> requested_exit_code{0};

    // 类 ROS2：单一、外部驱动的执行器。
    // - threads=0：本服务不额外创建 worker 线程。
    // - 本 run() 循环通过 exec.spin_once() 驱动回调执行。
    wxz::core::Executor::Options exec_opts;
    exec_opts.threads = 0;
    wxz::core::Executor exec(exec_opts);
    (void)exec.start();
    wxz::core::Strand ingress_strand(exec);
    wxz::core::Strand arm_sdk_strand(exec);

    wxz::core::NodeBaseConfig node_cfg;
    node_cfg.service = "workstation_arm_control_service";
    node_cfg.type = "device.arm";
    node_cfg.version = sw_version;
    node_cfg.api_version = 1;
    node_cfg.schema_version = 1;
    node_cfg.domain = domain;
    node_cfg.health_file = cfg.health_file;
    node_cfg.capability_topic = cfg.capability_topic;
    node_cfg.fault_topic = cfg.fault_status_topic;
    node_cfg.heartbeat_topic = cfg.heartbeat_topic;
    node_cfg.health_period_ms = 1000;
    node_cfg.capability_period_ms = 1000;
    node_cfg.heartbeat_period_ms = cfg.heartbeat_period_ms;
    node_cfg.timesync_period_ms = cfg.timesync_period_ms;
    node_cfg.timesync_scope = cfg.timesync_scope;
    node_cfg.topics_pub = {cfg.status_dto_topic, cfg.fault_status_topic};
    node_cfg.topics_sub = {cfg.cmd_dto_topic, cfg.fault_action_topic};
    node_cfg.warn = [&](const std::string& m) { logger.log(LogLevel::Warn, m); };

    wxz::workstation::Node ws_node(wxz::workstation::Node::Options{
        std::move(node_cfg),
        &exec,
        &ingress_strand,
        &logger,
        "workstation_arm_control_service",
    });
    ws_node.base().install_signal_handlers();

    auto metrics_export = maybe_start_metrics_export(ws_node.base(), logger, "workstation_arm_control_service");
    auto fault_recovery = maybe_start_fault_recovery(exec,
                                                     ws_node.base(),
                                                     logger,
                                                     domain,
                                                     cfg.fault_status_topic,
                                                     "workstation_arm_control_service",
                                                     requested_exit_code);

    logger.log(LogLevel::Info,
               "start ip=" + conn.ip + " port=" + std::to_string(conn.port) +
                   " domain=" + std::to_string(domain) +
                   " cmd='" + cfg.cmd_dto_topic + "' status='" + cfg.status_dto_topic + "'");

    CmdQueue queue(queue_max);

    // SDK 为直接链接依赖：若运行环境缺少 SDK runtime libs，则进程会在启动阶段被 loader 阻止（不会走到这里）。
    logger.log(LogLevel::Info, "SDK enabled (direct-linked)");
    std::unique_ptr<IArmClient> arm = std::make_unique<ArmSdkClient>(conn);

    // 业务处理器：KV 命令负载 -> KV 状态负载。
    ArmCommandProcessor processor;

    const ArmControlTopics topics{
        .domain = domain,
        .cmd_dto_topic = cfg.cmd_dto_topic,
        .cmd_dto_schema = cfg.cmd_dto_schema,
        .status_dto_topic = cfg.status_dto_topic,
        .status_dto_schema = cfg.status_dto_schema,
        .fault_action_topic = cfg.fault_action_topic,
        .dto_max_payload = cfg.dto_max_payload,
        .dto_source = cfg.dto_source,
    };

    auto status_pub = ws_node.create_publisher_eventdto(cfg.status_dto_topic, cfg.dto_max_payload);

    auto rpc_server = wxz::workstation::arm_control::internal::start_arm_rpc_control_plane(
        cfg,
        processor,
        *arm,
        arm_sdk_strand,
        logger);

    ArmControlLoop loop(ws_node,
                        exec,
                        arm_sdk_strand,
                        processor,
                        *arm,
                        queue,
                        *status_pub,
                        topics,
                        ArmControlLoop::Options{
                            .metrics_scope = cfg.metrics_scope,
                            .queue_max = queue_max,
                        },
                        logger);
    loop.run();

    if (rpc_server) rpc_server->stop();
    exec.stop();

    if (fault_recovery) fault_recovery->stop();

    logger.log(LogLevel::Info, "stop");
    const int code = requested_exit_code.load();
    return (code == 0) ? 0 : code;
}

} // namespace wxz::workstation::arm_control
