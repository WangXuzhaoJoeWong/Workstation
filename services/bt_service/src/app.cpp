#include "app.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include <behaviortree_cpp_v3/bt_factory.h>

#include "node_base.h"
#include "service_common.h"
#include "logger.h"

#include "byte_buffer_pool.h"
#include "executor.h"
#include "strand.h"

#include "app_config.h"
#include "arm_wiring.h"
#include "arm_types.h"
#include "bt_runtime_wiring.h"
#include "bt_tree_runner.h"
#include "dds_channels.h"
#include "fault_recovery_executor.h"
#include "main_loop.h"
#include "metrics_http_server.h"
#include "metrics_prometheus.h"
#include "node_wiring.h"
#include "rpc_control_plane.h"

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

    // Optional: HTTP /metrics endpoint
    {
        const int http_enable = wxz::core::getenv_int("WXZ_METRICS_HTTP_ENABLE", 0);
        if (http_enable) {
            wxz::core::MetricsHttpServer::Options o;
            o.bind_addr = wxz::core::getenv_str("WXZ_METRICS_HTTP_ADDR", "0.0.0.0");
            o.port = wxz::core::getenv_int("WXZ_METRICS_HTTP_PORT", 9100);
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
            // trim spaces
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

static int bt_service_main_impl() {
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);

    const auto cfg = wxz::workstation::bt_service::load_app_config_from_env();

    const auto log_level = wxz::core::parse_log_level(wxz::core::getenv_str("WXZ_LOG_LEVEL", "info"));
    auto& logger = wxz::core::Logger::getInstance();
    logger.set_level(log_level);
    logger.set_prefix("[workstation_bt_service] ");

    std::atomic<int> requested_exit_code{0};

    wxz::core::NodeBase node(wxz::workstation::bt_service::make_bt_node_config(cfg, logger));
    node.install_signal_handlers();

    auto metrics_export = maybe_start_metrics_export(node, logger, "workstation_bt_service");

    logger.log(wxz::core::LogLevel::Info,
               "start domain=" + std::to_string(cfg.domain) + " xml='" + cfg.bt.xml_path + "' tick_ms=" +
                   std::to_string(cfg.bt.tick_ms) + " reload_ms=" + std::to_string(cfg.bt.reload_ms));

    wxz::workstation::bt_service::ArmRespCache arm_cache;
    wxz::workstation::bt_service::TraceContext trace_ctx;

    // ROS2 风格：订阅回调统一投递到同一个由外部驱动的 Executor。
    // 主循环集中执行 NodeBase 的 publish/tick，避免多处并发驱动。
    wxz::core::Executor::Options exec_opts;
    exec_opts.threads = 0;
    wxz::core::Executor exec(exec_opts);
    (void)exec.start();
    wxz::core::Strand arm_status_ingress_strand(exec);
    wxz::core::Strand rpc_strand(exec);

    auto fault_recovery = maybe_start_fault_recovery(exec,
                                                     node,
                                                     logger,
                                                     cfg.domain,
                                                     cfg.fault_status_topic,
                                                     "workstation_bt_service",
                                                     requested_exit_code);

    const std::size_t arm_status_pool_buffers = static_cast<std::size_t>(
        std::max(1, wxz::core::getenv_int("WXZ_BT_ARM_STATUS_INGRESS_POOL_BUFFERS", 128)));
    wxz::core::ByteBufferPool arm_status_ingress_pool(wxz::core::ByteBufferPool::Options{
        .buffers = arm_status_pool_buffers,
        .buffer_capacity = cfg.dto.max_payload,
    });

    {
        auto channels = wxz::workstation::bt_service::make_dds_channels(cfg);

        BT::BehaviorTreeFactory factory;
        wxz::workstation::bt_service::setup_arm_control_bt(factory,
                                                          cfg,
                                                          channels,
                                                          arm_status_ingress_pool,
                                                          arm_status_ingress_strand,
                                                          arm_cache,
                                                          trace_ctx);

        auto tree_runner = wxz::workstation::bt_service::make_bt_tree_runner(factory, cfg.bt, logger);

        auto rpc_server = wxz::workstation::bt_service::start_bt_rpc_control_plane(cfg, node, *tree_runner, rpc_strand, logger);

        wxz::workstation::bt_service::run_bt_main_loop(node, exec, *tree_runner, cfg.bt.tick_ms);

        if (rpc_server) rpc_server->stop();
    }

    exec.stop();

    if (fault_recovery) fault_recovery->stop();

    logger.log(wxz::core::LogLevel::Info, "stop");
    const int code = requested_exit_code.load();
    return (code == 0) ? 0 : code;
}

namespace wxz::workstation::bt_service {

int run() {
    return bt_service_main_impl();
}

}  // namespace wxz::workstation::bt_service
