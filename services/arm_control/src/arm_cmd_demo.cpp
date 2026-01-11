#include "wxz_workstation/arm_control/internal/arm_command_handler.h"

#include "wxz_workstation/arm_control/internal/arm_error_codes.h"

#include <string>

namespace wxz::workstation::arm_control::internal {

// Example command module showing the preferred "C++ class" style.
//
// How to use:
// - Publish on /arm/command (DTO payload):  op=demo_echo;id=1;msg=hello
// - Observe on /arm/status (DTO payload): ok=1;echo=hello
//
// Notes:
// - This handler intentionally does NOT touch the robot SDK, so it is safe to run
//   without hardware connected.
// - For real commands, implement a class with private helpers and call methods on
//   IArmClient (e.g., ArmSdkClient) inside the handler.
class ArmCmdDemo {
public:
    static EventDTOUtil::KvMap handle_echo(const ArmCommand& cmd, IArmClient& /*arm*/, const Logger& logger) {
        EventDTOUtil::KvMap resp;
        if (!cmd.id.empty()) resp["id"] = cmd.id;
        resp["op"] = cmd.op;

        const auto& kv = cmd.kv;
        const std::string msg = kv.count("msg") ? kv.at("msg") : "";
        logger.log(LogLevel::Info, std::string("demo_echo msg='") + msg + "'");

        resp["echo"] = msg;
        arm_set_ok(resp);
        return resp;
    }
};

// Static registration (no dlopen). This is the template you can copy.
namespace {
struct ArmCmdDemoRegistrar {
    ArmCmdDemoRegistrar() {
        register_arm_handler("demo_echo", &ArmCmdDemo::handle_echo);
    }
};

static ArmCmdDemoRegistrar g_registrar;
} // namespace

} // namespace wxz::workstation::arm_control::internal
