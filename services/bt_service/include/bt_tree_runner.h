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

#include "app_config.h"
#include "logger.h"

namespace wxz::workstation::bt_service {

/// 行为树热加载结果。
enum class TreeReloadResult { Ok, Unchanged, ReadError, ParseError };

/// 行为树运行器：负责加载 XML、按周期热加载、并 tick 行为树。
class BtTreeRunner {
public:
    /// 构造并加载 XML。
    BtTreeRunner(BT::BehaviorTreeFactory& factory, std::string xml_path, int reload_ms, const wxz::core::Logger& logger);

    ~BtTreeRunner();

    BtTreeRunner(const BtTreeRunner&) = delete;
    BtTreeRunner& operator=(const BtTreeRunner&) = delete;
    BtTreeRunner(BtTreeRunner&&) noexcept = default;
    BtTreeRunner& operator=(BtTreeRunner&&) noexcept = default;

    /// 如果 XML 文件发生变化则尝试重载，并返回结果。
    TreeReloadResult reload_if_changed();

    /// 在满足 reload_ms 的节流条件下尝试重载（内部调用 reload_if_changed）。
    void maybe_reload();

    /// tick 一次行为树（单步执行）。
    void tick_once();

    /// 配置 Groot1 发布器（若编译时支持）。
    void configure_groot1(const Groot1Config& cfg);

private:
    BT::BehaviorTreeFactory* factory_;
    std::string xml_path_;
    int reload_ms_;
    const wxz::core::Logger* logger_;

    std::string last_xml_;
    BT::Tree tree_;
    std::chrono::steady_clock::time_point last_reload_{std::chrono::steady_clock::now()};

    bool read_error_reported_{false};

#if WXZ_BT_HAS_GROOT1
    std::unique_ptr<BT::PublisherZMQ> zmq_pub_;
#endif
};

}  // namespace wxz::workstation::bt_service
