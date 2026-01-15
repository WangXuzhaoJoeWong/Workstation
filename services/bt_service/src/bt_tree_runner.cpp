#include "bt_tree_runner.h"

#include <algorithm>
#include <exception>
#include <string>

#if WXZ_BT_HAS_GROOT1
#include <behaviortree_cpp_v3/loggers/bt_zmq_publisher.h>
#endif

#include "arm_types.h"

namespace wxz::workstation::bt_service {

BtTreeRunner::BtTreeRunner(BT::BehaviorTreeFactory& factory,
                           std::string xml_path,
                           int reload_ms,
                           const wxz::core::Logger& logger)
    : factory_(&factory), xml_path_(std::move(xml_path)), reload_ms_(reload_ms), logger_(&logger) {}

BtTreeRunner::~BtTreeRunner() = default;

TreeReloadResult BtTreeRunner::reload_if_changed() {
    std::string xml;
    if (!load_text_file(xml_path_, xml)) {
        if (!read_error_reported_) {
            logger_->log(wxz::core::LogLevel::Error, std::string("failed to read xml: ") + xml_path_);
            read_error_reported_ = true;
        }
        return TreeReloadResult::ReadError;
    }

    read_error_reported_ = false;

    if (xml == last_xml_) return TreeReloadResult::Unchanged;

    try {
        tree_ = factory_->createTreeFromText(xml);
        last_xml_ = std::move(xml);
        logger_->log(wxz::core::LogLevel::Info, "tree loaded");
        return TreeReloadResult::Ok;
    } catch (const std::exception& e) {
        logger_->log(wxz::core::LogLevel::Error, std::string("tree load error: ") + e.what());
        return TreeReloadResult::ParseError;
    }
}

void BtTreeRunner::maybe_reload() {
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_reload_).count() >= reload_ms_) {
        (void)reload_if_changed();
        last_reload_ = now;
    }
}

void BtTreeRunner::tick_once() {
    if (tree_.rootNode()) {
        (void)tree_.tickRoot();
    }
}

void BtTreeRunner::configure_groot1(const Groot1Config& cfg) {
#if WXZ_BT_HAS_GROOT1
    zmq_pub_.reset();
    if (!cfg.enable) return;

    if (!tree_.rootNode()) {
        logger_->log(wxz::core::LogLevel::Warn, "Groot1 requested but tree not loaded; skip Groot1 init");
        return;
    }

    int port = cfg.port;
    int server_port = (cfg.server_port > 0) ? cfg.server_port : (cfg.port + 1);
    const int max_msg_per_second = std::max(1, cfg.max_msg_per_sec);

    for (int attempt = 0; attempt <= cfg.retry; ++attempt) {
        try {
            zmq_pub_ = std::make_unique<BT::PublisherZMQ>(tree_,
                                                         static_cast<uint16_t>(max_msg_per_second),
                                                         static_cast<uint16_t>(port),
                                                         static_cast<uint16_t>(server_port));
            logger_->log(wxz::core::LogLevel::Info,
                         "Groot1 enabled on port " + std::to_string(port) + " (server " + std::to_string(server_port) + ")");
            logger_->log(wxz::core::LogLevel::Info, "Groot1 max_msg_per_second=" + std::to_string(max_msg_per_second));
            break;
        } catch (const std::exception& e) {
            if (attempt < cfg.retry) {
                logger_->log(wxz::core::LogLevel::Warn, std::string("Groot1 init failed, retry next port: ") + e.what());
                ++port;
                ++server_port;
                continue;
            }
            logger_->log(wxz::core::LogLevel::Warn, std::string("Groot1 init failed (ignored): ") + e.what());
        }
    }
#else
    (void)cfg;
#endif
}

}  // namespace wxz::workstation::bt_service
