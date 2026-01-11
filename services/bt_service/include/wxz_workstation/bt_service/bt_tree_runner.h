#pragma once

#include <chrono>
#include <memory>
#include <string>

#include <behaviortree_cpp_v3/bt_factory.h>

#if __has_include(<behaviortree_cpp_v3/loggers/bt_zmq_publisher.h>)
#define WXZ_BT_HAS_GROOT1 1
namespace BT {
class PublisherZMQ;
}
#else
#define WXZ_BT_HAS_GROOT1 0
#endif

#include "wxz_workstation/bt_service/app_config.h"
#include "logger.h"

namespace wxz::workstation::bt_service {

enum class TreeReloadResult { Ok, Unchanged, ReadError, ParseError };

class BtTreeRunner {
public:
    BtTreeRunner(BT::BehaviorTreeFactory& factory, std::string xml_path, int reload_ms, const wxz::core::Logger& logger);

    ~BtTreeRunner();

    BtTreeRunner(const BtTreeRunner&) = delete;
    BtTreeRunner& operator=(const BtTreeRunner&) = delete;
    BtTreeRunner(BtTreeRunner&&) noexcept = default;
    BtTreeRunner& operator=(BtTreeRunner&&) noexcept = default;

    TreeReloadResult reload_if_changed();
    void maybe_reload();
    void tick_once();

    void configure_groot1(const Groot1Config& cfg);

private:
    BT::BehaviorTreeFactory* factory_;
    std::string xml_path_;
    int reload_ms_;
    const wxz::core::Logger* logger_;

    std::string last_xml_;
    BT::Tree tree_;
    std::chrono::steady_clock::time_point last_reload_{std::chrono::steady_clock::now()};

#if WXZ_BT_HAS_GROOT1
    std::unique_ptr<BT::PublisherZMQ> zmq_pub_;
#endif
};

}  // namespace wxz::workstation::bt_service
