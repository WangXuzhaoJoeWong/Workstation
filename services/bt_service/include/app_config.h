#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace wxz::workstation::bt_service {

/// 机械臂控制相关配置（cmd/status topic 与超时）。
struct ArmConfig {
    std::string cmd_dto_topic;
    std::string cmd_dto_schema;
    std::string status_dto_topic;
    std::uint64_t timeout_ms{30000};
};

/// 系统告警 DTO 发布相关配置。
struct SystemAlertConfig {
    std::string dto_topic;
    std::string dto_schema;
};

/// Groot1（BehaviorTree.CPP 可视化/调试工具）相关配置。
struct Groot1Config {
    int enable{1};
    int port{1666};
    int server_port{-1};
    int retry{5};
    int max_msg_per_sec{25};
};

/// 行为树运行相关配置。
struct BtConfig {
    std::string xml_path;
    int tick_ms{20};
    int reload_ms{500};
    Groot1Config groot;
};

/// DTO 公共配置。
struct DtoConfig {
    std::string source;
    std::size_t max_payload{8192};
};

/// bt_service 的 RPC 控制面配置。
///
/// 注意：当 enable=0 时，控制面不启动；topic/service_name 也可留空。
struct RpcConfig {
    // 0：禁用（默认，更安全）
    // 1：启用
    int enable{0};
    std::string request_topic;
    std::string reply_topic;
    std::string service_name;
};

/// bt_service 总配置。
struct AppConfig {
    int domain{0};

    std::string capability_topic;
    std::string fault_status_topic;
    std::string heartbeat_topic;
    int heartbeat_period_ms{1000};
    int timesync_period_ms{5000};
    std::string timesync_scope;
    std::string health_file;
    std::string sw_version;

    ArmConfig arm;
    SystemAlertConfig system_alert;
    BtConfig bt;
    DtoConfig dto;
    RpcConfig rpc;
};

/// 从环境变量/默认值加载 bt_service 配置。
AppConfig load_app_config_from_env();

}  // namespace wxz::workstation::bt_service
