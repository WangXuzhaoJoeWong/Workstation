#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

#include "logger.h"

#include "workstation/node.h"

namespace wxz::core {
class Executor;
class Strand;
} // namespace wxz::core

namespace wxz::workstation::arm_control::internal {

class ArmCommandProcessor;
class IArmClient;
class CmdQueue;

struct ArmControlTopics {
    int domain{0};

    // 仅 DTO topics（FastDDS 负载为 EventDTO 的 CDR 字节流）
    std::string cmd_dto_topic;
    std::string cmd_dto_schema;
    std::string status_dto_topic;
    std::string status_dto_schema;

    std::string fault_action_topic{"fault/action"};
    std::size_t dto_max_payload{8 * 1024};
    std::string dto_source{"workstation_arm_control_service"};
};

class ArmControlLoop {
public:
    struct Options {
        std::string metrics_scope{"workstation_arm_control_service"};
        std::size_t queue_max{64};
    };

    ArmControlLoop(wxz::workstation::Node& node,
                  wxz::core::Executor& exec,
                  wxz::core::Strand& arm_sdk_strand,
                  ArmCommandProcessor& processor,
                  IArmClient& arm,
                  CmdQueue& queue,
                  wxz::workstation::EventDtoPublisher& status_pub,
                  ArmControlTopics topics,
                  Options opts,
                  wxz::core::Logger& logger);

    void run(std::chrono::milliseconds pop_timeout = std::chrono::milliseconds(200));

private:
    wxz::workstation::Node& node_;
    wxz::core::Executor& exec_;
    wxz::core::Strand& arm_sdk_strand_;
    ArmCommandProcessor& processor_;
    IArmClient& arm_;
    CmdQueue& queue_;
    wxz::workstation::EventDtoPublisher& status_pub_;
    ArmControlTopics topics_;
    Options opts_;
    wxz::core::Logger& logger_;
};

} // namespace wxz::workstation::arm_control::internal
