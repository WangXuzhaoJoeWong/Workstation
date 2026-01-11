#include "wxz_workstation/arm_control/internal/arm_command_handler.h"

#include <cstdlib>
#include <iostream>
#include <string>

#include "wxz_workstation/arm_control/internal/arm_error_codes.h"

namespace wxz::workstation::arm_control::internal {

namespace {
std::string kv_get(const EventDTOUtil::KvMap& kv, const char* key) {
    auto it = kv.find(key);
    if (it == kv.end()) return "";
    return it->second;
}

int expect_eq(const char* label, const std::string& got, const std::string& exp) {
    if (got == exp) return 0;
    std::cerr << "[FAIL] " << label << ": expected '" << exp << "' got '" << got << "'\n";
    return 1;
}

int expect_ok0_missing(const char* case_name, const EventDTOUtil::KvMap& resp, const std::string& missing_key) {
    int fails = 0;
    fails += expect_eq((std::string(case_name) + ":ok").c_str(), kv_get(resp, "ok"), "0");
    fails += expect_eq((std::string(case_name) + ":err_code").c_str(),
                       kv_get(resp, "err_code"),
                       std::to_string(static_cast<int>(ArmErrc::MissingField)));
    fails += expect_eq((std::string(case_name) + ":err").c_str(), kv_get(resp, "err"), "missing_" + missing_key);
    return fails;
}

class DummyArmClient final : public IArmClient {
public:
    CRresult moveL(const std::array<double, 6>&,
                   const std::array<double, 6>&,
                   double,
                   double,
                   double) override {
        return success;
    }

    CRresult moveJ(const std::array<double, 6>&, double) override { return success; }

    CRresult power_on_enable(Logger const&) override { return success; }

    CRresult get_robot_mode(int& out_mode) override {
        out_mode = 42;
        return success;
    }

    CRresult fault_reset() override { return success; }

    CRresult slow_speed(bool) override { return success; }

    CRresult quick_stop(bool) override { return success; }

    CRresult path_download(const std::string&, int, int, std::size_t) override { return success; }
};

} // namespace

int run_offline_selftest() {
    auto& logger = wxz::core::Logger::getInstance();
    logger.set_level(LogLevel::Debug);
    logger.set_prefix("[wxz_arm_offline_selftest] ");

    DummyArmClient arm;

    int fails = 0;

    // 1) Missing op => MissingField/missing_op
    {
        const ArmCommand cmd = parse_arm_command("id=1");
        const auto resp = handle_arm_command(cmd, arm, logger);
        fails += expect_ok0_missing("missing_op", resp, "op");
    }

    // 2) moveL missing pose => MissingField/missing_pose
    {
        const ArmCommand cmd = parse_arm_command("op=moveL;id=1;jointpos=1,2,3,4,5,6");
        const auto resp = handle_arm_command(cmd, arm, logger);
        fails += expect_ok0_missing("moveL_missing_pose", resp, "pose");
    }

    // 3) demo_echo missing msg => MissingField/missing_msg
    {
        const ArmCommand cmd = parse_arm_command("op=demo_echo;id=1");
        const auto resp = handle_arm_command(cmd, arm, logger);
        fails += expect_ok0_missing("demo_echo_missing_msg", resp, "msg");
    }

    // 4) quickStop missing enable => MissingField/missing_enable
    {
        const ArmCommand cmd = parse_arm_command("op=quickStop;id=1");
        const auto resp = handle_arm_command(cmd, arm, logger);
        fails += expect_ok0_missing("quickStop_missing_enable", resp, "enable");
    }

    // 5) demo_echo ok => ok=1, err_code=0, echo matches
    {
        const ArmCommand cmd = parse_arm_command("op=demo_echo;id=9;msg=hello");
        const auto resp = handle_arm_command(cmd, arm, logger);
        fails += expect_eq("demo_echo_ok:ok", kv_get(resp, "ok"), "1");
        fails += expect_eq("demo_echo_ok:err_code",
                           kv_get(resp, "err_code"),
                           std::to_string(static_cast<int>(ArmErrc::Ok)));
        fails += expect_eq("demo_echo_ok:echo", kv_get(resp, "echo"), "hello");
    }

    if (fails == 0) {
        std::cout << "[PASS] wxz_arm_control_offline_selftest\n";
        return 0;
    }
    std::cerr << "[FAIL] failures=" << fails << "\n";
    return 1;
}

} // namespace wxz::workstation::arm_control::internal

int main() {
    return wxz::workstation::arm_control::internal::run_offline_selftest();
}
