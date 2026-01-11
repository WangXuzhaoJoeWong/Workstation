#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace wxz::workstation::bt_service {

struct ArmConfig {
    std::string cmd_dto_topic;
    std::string cmd_dto_schema;
    std::string status_dto_topic;
    std::uint64_t timeout_ms{30000};
};

struct SystemAlertConfig {
    std::string dto_topic;
    std::string dto_schema;
};

struct Groot1Config {
    int enable{1};
    int port{1666};
    int server_port{-1};
    int retry{5};
    int max_msg_per_sec{25};
};

struct BtConfig {
    std::string xml_path;
    int tick_ms{20};
    int reload_ms{500};
    Groot1Config groot;
};

struct DtoConfig {
    std::string source;
    std::size_t max_payload{8192};
};

struct AppConfig {
    int domain{0};

    std::string capability_topic;
    std::string health_file;
    std::string sw_version;

    ArmConfig arm;
    SystemAlertConfig system_alert;
    BtConfig bt;
    DtoConfig dto;
};

AppConfig load_app_config_from_env();

}  // namespace wxz::workstation::bt_service
