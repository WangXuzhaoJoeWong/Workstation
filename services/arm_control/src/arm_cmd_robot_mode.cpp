#include "wxz_workstation/arm_control/internal/arm_command_handler.h"

#include "wxz_workstation/arm_control/internal/arm_error_codes.h"

#include <string>

namespace wxz::workstation::arm_control::internal {

// A SDK-like C++ class command module (similar spirit to CGXi demo CgxRobot).
//
// Publish on /arm/command (DTO payload):
//   op=robot_mode;id=1
//
// Observe on /arm/status (DTO payload):
//   op=robot_mode;id=1;ok=1;mode=<int>
class ArmCmdRobotMode {
public:
    static EventDTOUtil::KvMap handle(const ArmCommand& cmd, IArmClient& arm, const Logger& logger) {
        EventDTOUtil::KvMap resp;
        if (!cmd.id.empty()) resp["id"] = cmd.id;
        resp["op"] = cmd.op;

        int mode = 0;
        const CRresult r = arm.get_robot_mode(mode);
        arm_set_sdk_result(resp, static_cast<int>(r));
        if (r == success) {
            resp["mode"] = std::to_string(mode);
            logger.log(LogLevel::Info, std::string("robot_mode=") + resp["mode"]);
        } else {
            logger.log(LogLevel::Error, std::string("robot_mode query failed code=") + resp["code"]);
        }
        return resp;
    }
};

namespace {
struct ArmCmdRobotModeRegistrar {
    ArmCmdRobotModeRegistrar() {
        register_arm_handler("robot_mode", &ArmCmdRobotMode::handle);
    }
};

static ArmCmdRobotModeRegistrar g_registrar;
} // namespace

} // namespace wxz::workstation::arm_control::internal
