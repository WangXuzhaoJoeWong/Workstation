#include "wxz_workstation/arm_control/internal/arm_command_handler.h"
#include <array>
#include <iomanip>
#include <sstream>
#include <unordered_map>

#include "wxz_workstation/arm_control/internal/arm_error_codes.h"
#include "command_router.h"
#include "kv_codec.h"

namespace wxz::workstation::arm_control::internal {

ArmCommand parse_arm_command(const std::string& raw) {
    ArmCommand cmd;
    cmd.raw = raw;
    cmd.kv = wxz::core::KvCodec::parse(raw);
    cmd.op = wxz::core::KvCodec::get(cmd.kv, "op", "");
    cmd.id = wxz::core::KvCodec::get(cmd.kv, "id", "");
    return cmd;
}

static EventDTOUtil::KvMap make_base_resp(const ArmCommand& cmd) {
    EventDTOUtil::KvMap resp;
    if (!cmd.id.empty()) resp["id"] = cmd.id;
    resp["op"] = cmd.op;
    return resp;
}
// Registry singleton
static std::unordered_map<std::string, ArmCommandHandler>& registry() {
    static auto* m = new std::unordered_map<std::string, ArmCommandHandler>();
    return *m;
}

void register_arm_handler(const std::string& op, ArmCommandHandler fn) {
    registry()[op] = fn;
}

// Built-in handlers
static EventDTOUtil::KvMap h_moveL(const ArmCommand& cmd, IArmClient& arm, const Logger& logger) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    const auto& kv = cmd.kv;
    const std::string pose_s = kv.count("pose") ? kv.at("pose") : "";
    const std::string joint_s = kv.count("jointpos") ? kv.at("jointpos") : "";
    const double speed = kv.count("speed") ? parse_double(kv.at("speed")).value_or(30.0) : 30.0;
    const double acc = kv.count("acc") ? parse_double(kv.at("acc")).value_or(30.0) : 30.0;
    const double jerk = kv.count("jerk") ? parse_double(kv.at("jerk")).value_or(60.0) : 60.0;
    auto pose = parse_csv6(pose_s);
    auto joint = parse_csv6(joint_s);
    if (!pose || !joint) {
        logger.log(LogLevel::Warn, "moveL bad pose or jointpos");
        arm_set_error(resp, ArmErrc::ParseError, "bad_pose_or_jointpos");
        return resp;
    }
    const CRresult r = arm.moveL(*joint, *pose, speed, acc, jerk);
    arm_set_sdk_result(resp, static_cast<int>(r));
    if (r != success) logger.log(LogLevel::Error, "moveL failed code=" + std::to_string(static_cast<int>(r)));
    return resp;
}

static EventDTOUtil::KvMap h_moveJoint(const ArmCommand& cmd, IArmClient& arm, const Logger& logger) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    const auto& kv = cmd.kv;
    const std::string joint_s = kv.count("jointpos") ? kv.at("jointpos") : "";
    const double speed = kv.count("speed") ? parse_double(kv.at("speed")).value_or(3.14) : 3.14;
    auto joint = parse_csv6(joint_s);
    if (!joint) {
        logger.log(LogLevel::Warn, "moveJoint bad jointpos");
        arm_set_error(resp, ArmErrc::ParseError, "bad_jointpos");
        return resp;
    }
    const CRresult r = arm.moveJ(*joint, speed);
    arm_set_sdk_result(resp, static_cast<int>(r));
    if (r != success) logger.log(LogLevel::Error, "moveJoint failed code=" + std::to_string(static_cast<int>(r)));
    return resp;
}

static EventDTOUtil::KvMap h_power_on(const ArmCommand& cmd, IArmClient& arm, const Logger& logger) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    const CRresult r = arm.power_on_enable(logger);
    arm_set_sdk_result(resp, static_cast<int>(r));
    if (r != success) logger.log(LogLevel::Error, "power_on failed code=" + std::to_string(static_cast<int>(r)));
    return resp;
}

static EventDTOUtil::KvMap h_fault_reset(const ArmCommand& cmd, IArmClient& arm, const Logger& logger) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    const CRresult r = arm.fault_reset();
    arm_set_sdk_result(resp, static_cast<int>(r));
    if (r != success) logger.log(LogLevel::Error, "fault_reset failed code=" + std::to_string(static_cast<int>(r)));
    return resp;
}

static EventDTOUtil::KvMap h_slowSpeed(const ArmCommand& cmd, IArmClient& arm, const Logger& /*logger*/) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    const auto& kv = cmd.kv;
    const bool enable = kv.count("enable") ? (kv.at("enable") == "1" || kv.at("enable") == "true") : true;
    const CRresult r = arm.slow_speed(enable);
    arm_set_sdk_result(resp, static_cast<int>(r));
    return resp;
}

static EventDTOUtil::KvMap h_quickStop(const ArmCommand& cmd, IArmClient& arm, const Logger& /*logger*/) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    const auto& kv = cmd.kv;
    const bool enable = kv.count("enable") ? (kv.at("enable") == "1" || kv.at("enable") == "true") : true;
    const CRresult r = arm.quick_stop(enable);
    arm_set_sdk_result(resp, static_cast<int>(r));
    return resp;
}

static EventDTOUtil::KvMap h_path_download(const ArmCommand& cmd, IArmClient& arm, const Logger& logger) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    const auto& kv = cmd.kv;
    const std::string file = kv.count("file") ? kv.at("file") : "";
    const int index = kv.count("index") ? parse_int(kv.at("index")).value_or(1) : 1;
    const int move_type = kv.count("moveType") ? parse_int(kv.at("moveType")).value_or(1) : 1;
    const std::size_t max_points = kv.count("maxPoints") ? parse_size(kv.at("maxPoints")).value_or(10000) : 10000;
    if (file.empty()) {
        logger.log(LogLevel::Warn, "path_download missing file");
        arm_set_error(resp, ArmErrc::MissingField, "missing_file");
        return resp;
    }
    const CRresult r = arm.path_download(file, index, move_type, max_points);
    arm_set_sdk_result(resp, static_cast<int>(r));
    if (r != success) logger.log(LogLevel::Error, "path_download failed code=" + std::to_string(static_cast<int>(r)));
    return resp;
}

static ArmSdkClient* as_sdk_client(IArmClient& arm) {
    return dynamic_cast<ArmSdkClient*>(&arm);
}

static std::string format_csv6_fixed(const std::array<double, 6>& v, int precision = 6) {
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os << std::setprecision(precision);
    os << v[0] << "," << v[1] << "," << v[2] << "," << v[3] << "," << v[4] << "," << v[5];
    return os.str();
}

static EventDTOUtil::KvMap h_is_arm_ready(const ArmCommand& cmd, IArmClient& arm, const Logger& /*logger*/) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    auto* sdk = as_sdk_client(arm);
    if (!sdk) {
        arm_set_error(resp, ArmErrc::InternalError, "unsupported_client");
        return resp;
    }
    resp["value"] = sdk->IsArmReady() ? "1" : "0";
    arm_set_ok(resp);
    return resp;
}

static EventDTOUtil::KvMap h_is_power_on(const ArmCommand& cmd, IArmClient& arm, const Logger& /*logger*/) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    auto* sdk = as_sdk_client(arm);
    if (!sdk) {
        arm_set_error(resp, ArmErrc::InternalError, "unsupported_client");
        return resp;
    }
    resp["value"] = sdk->IsPowerOn() ? "1" : "0";
    arm_set_ok(resp);
    return resp;
}

static EventDTOUtil::KvMap h_is_start_signal(const ArmCommand& cmd, IArmClient& arm, const Logger& /*logger*/) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    auto* sdk = as_sdk_client(arm);
    if (!sdk) {
        arm_set_error(resp, ArmErrc::InternalError, "unsupported_client");
        return resp;
    }
    resp["value"] = sdk->IsStartSignal() ? "1" : "0";
    arm_set_ok(resp);
    return resp;
}

static EventDTOUtil::KvMap h_is_stop_signal(const ArmCommand& cmd, IArmClient& arm, const Logger& /*logger*/) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    auto* sdk = as_sdk_client(arm);
    if (!sdk) {
        arm_set_error(resp, ArmErrc::InternalError, "unsupported_client");
        return resp;
    }
    resp["value"] = sdk->IsStopSignal() ? "1" : "0";
    arm_set_ok(resp);
    return resp;
}

static EventDTOUtil::KvMap h_is_trajectory_complete(const ArmCommand& cmd, IArmClient& arm, const Logger& /*logger*/) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    auto* sdk = as_sdk_client(arm);
    if (!sdk) {
        arm_set_error(resp, ArmErrc::InternalError, "unsupported_client");
        return resp;
    }
    resp["value"] = sdk->IsTrajectoryComplete() ? "1" : "0";
    arm_set_ok(resp);
    return resp;
}

static EventDTOUtil::KvMap h_is_all_trajectories_complete(const ArmCommand& cmd, IArmClient& arm, const Logger& /*logger*/) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    auto* sdk = as_sdk_client(arm);
    if (!sdk) {
        arm_set_error(resp, ArmErrc::InternalError, "unsupported_client");
        return resp;
    }
    resp["value"] = sdk->IsAllTrajectoriesComplete() ? "1" : "0";
    arm_set_ok(resp);
    return resp;
}

static EventDTOUtil::KvMap h_initialize_arm(const ArmCommand& cmd, IArmClient& arm, const Logger& logger) {
    // Alias to existing behavior.
    return h_power_on(cmd, arm, logger);
}

static EventDTOUtil::KvMap h_wait_for_start(const ArmCommand& cmd, IArmClient& arm, const Logger& logger) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    auto* sdk = as_sdk_client(arm);
    if (!sdk) {
        arm_set_error(resp, ArmErrc::InternalError, "unsupported_client");
        return resp;
    }

    const auto& kv = cmd.kv;
    const int timeout_ms = kv.count("timeout_ms") ? parse_int(kv.at("timeout_ms")).value_or(30000) : 30000;
    const CRresult r = sdk->WaitForStart(std::chrono::milliseconds(timeout_ms), logger);
    resp["value"] = (r == success) ? "1" : "0";
    // For this high-level op we report transport/handler success via ok=1,
    // and use value=0/1 to convey the BT outcome.
    arm_set_ok(resp);
    return resp;
}

static EventDTOUtil::KvMap h_execute_trajectory(const ArmCommand& cmd, IArmClient& arm, const Logger& logger) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    auto* sdk = as_sdk_client(arm);
    if (!sdk) {
        arm_set_error(resp, ArmErrc::InternalError, "unsupported_client");
        return resp;
    }

    const auto& kv = cmd.kv;
    const int timeout_ms = kv.count("timeout_ms") ? parse_int(kv.at("timeout_ms")).value_or(60000) : 60000;
    const CRresult r = sdk->ExecuteTrajectory(std::chrono::milliseconds(timeout_ms), logger);
    resp["value"] = (r == success) ? "1" : "0";
    arm_set_ok(resp);
    return resp;
}

static EventDTOUtil::KvMap h_emergency_stop(const ArmCommand& cmd, IArmClient& arm, const Logger& logger) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    auto* sdk = as_sdk_client(arm);
    if (!sdk) {
        arm_set_error(resp, ArmErrc::InternalError, "unsupported_client");
        return resp;
    }
    const CRresult r = sdk->EmergencyStop(logger);
    arm_set_sdk_result(resp, static_cast<int>(r));
    return resp;
}

static EventDTOUtil::KvMap h_reset_system(const ArmCommand& cmd, IArmClient& arm, const Logger& logger) {
    // Alias to fault_reset.
    return h_fault_reset(cmd, arm, logger);
}

static EventDTOUtil::KvMap h_get_joint_actual_pos(const ArmCommand& cmd, IArmClient& arm, const Logger& /*logger*/) {
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    auto* sdk = as_sdk_client(arm);
    if (!sdk) {
        arm_set_error(resp, ArmErrc::InternalError, "unsupported_client");
        return resp;
    }

    std::array<double, 6> pos_deg{};
    const CRresult r = sdk->GetJointActualPosDeg(pos_deg);
    arm_set_sdk_result(resp, static_cast<int>(r));
    if (r == success) {
        constexpr double kPi = 3.14159265358979323846;
        std::array<double, 6> pos_rad{};
        for (std::size_t i = 0; i < 6; ++i) pos_rad[i] = pos_deg[i] * kPi / 180.0;
        // Convention: jointpos is radians to align with MoveJ/MoveL inputs.
        resp["jointpos"] = format_csv6_fixed(pos_rad);
        resp["jointpos_deg"] = format_csv6_fixed(pos_deg);
    }
    return resp;
}

void init_default_arm_handlers() {
    // idempotent registration (override is fine for same fn)
    register_arm_handler("moveL", &h_moveL);
    register_arm_handler("moveLine", &h_moveL);
    register_arm_handler("moveJoint", &h_moveJoint);
    register_arm_handler("moveJ", &h_moveJoint);
    register_arm_handler("power_on", &h_power_on);
    // Alias: keep wording consistent with bt_service / user expectation.
    register_arm_handler("power_on_enable", &h_power_on);
    register_arm_handler("initialize_arm", &h_initialize_arm);
    register_arm_handler("fault_reset", &h_fault_reset);
    register_arm_handler("reset_system", &h_reset_system);
    register_arm_handler("slowSpeed", &h_slowSpeed);
    register_arm_handler("slow_speed", &h_slowSpeed);
    register_arm_handler("quickStop", &h_quickStop);
    register_arm_handler("quick_stop", &h_quickStop);
    register_arm_handler("path_download", &h_path_download);

    // High-level status/signal helpers.
    register_arm_handler("is_arm_ready", &h_is_arm_ready);
    register_arm_handler("is_power_on", &h_is_power_on);
    register_arm_handler("is_start_signal", &h_is_start_signal);
    register_arm_handler("is_stop_signal", &h_is_stop_signal);
    register_arm_handler("is_trajectory_complete", &h_is_trajectory_complete);
    register_arm_handler("is_all_trajectories_complete", &h_is_all_trajectories_complete);
    register_arm_handler("wait_for_start", &h_wait_for_start);
    register_arm_handler("execute_trajectory", &h_execute_trajectory);
    register_arm_handler("emergency_stop", &h_emergency_stop);

    // Query actual joint position.
    // - jointpos: radians (for MoveJ/MoveL)
    // - jointpos_deg: degrees (for debugging)
    register_arm_handler("get_joint_actual_pos", &h_get_joint_actual_pos);
}

EventDTOUtil::KvMap handle_arm_command(const ArmCommand& cmd, IArmClient& arm, const Logger& logger) {
    static bool initialized = false;
    if (!initialized) {
        init_default_arm_handlers();
        initialized = true;
    }

    // Build a router on-demand so it always reflects the current registry()
    // (including module-based registrations).
    wxz::core::CommandRouter router;
    EventDTOUtil::KvMap out;
    bool out_set = false;

    auto set_out = [&](EventDTOUtil::KvMap resp) {
        out = std::move(resp);
        out_set = true;
    };

    router.on_missing_op = [&](const wxz::core::CommandRouter::KvMap& kv) {
        ArmCommand c;
        c.kv = kv;
        c.op = wxz::core::KvCodec::get(kv, "op", "");
        c.id = wxz::core::KvCodec::get(kv, "id", "");
        EventDTOUtil::KvMap resp = make_base_resp(c);
        logger.log(LogLevel::Warn, "missing op");
        arm_set_error(resp, ArmErrc::MissingField, "missing_op");
        set_out(std::move(resp));
    };

    router.on_unknown_op = [&](std::string_view op, const wxz::core::CommandRouter::KvMap& kv) {
        ArmCommand c;
        c.kv = kv;
        c.op = std::string(op);
        c.id = wxz::core::KvCodec::get(kv, "id", "");
        EventDTOUtil::KvMap resp = make_base_resp(c);
        logger.log(LogLevel::Warn, "unknown op='" + std::string(op) + "'");
        arm_set_error(resp, ArmErrc::UnknownOp, "unknown_op");
        set_out(std::move(resp));
    };

    router.on_missing_field = [&](std::string_view op, std::string_view missing_key, const wxz::core::CommandRouter::KvMap& kv) {
        ArmCommand c;
        c.kv = kv;
        c.op = op.empty() ? wxz::core::KvCodec::get(kv, "op", "") : std::string(op);
        c.id = wxz::core::KvCodec::get(kv, "id", "");
        EventDTOUtil::KvMap resp = make_base_resp(c);
        logger.log(LogLevel::Warn,
                   "missing field: op='" + c.op + "' key='" + std::string(missing_key) + "'");
        arm_set_error(resp, ArmErrc::MissingField, "missing_" + std::string(missing_key));
        set_out(std::move(resp));
    };

    // For arm_control we keep the historical behavior: do not hard-require id.
    // (Some external controllers may omit it; we still return a best-effort response.)
    for (const auto& [op, fn] : registry()) {
        if (!fn) continue;

        // Route-specific required fields.
        // NOTE: We only enforce for core built-in ops to avoid breaking third-party/module ops.
        // - moveL/moveLine: requires pose + jointpos
        // - moveJoint: requires jointpos
        // - path_download: requires file
        // - demo_echo: requires msg
        // - slowSpeed/quickStop: requires enable
        if (op == "moveL" || op == "moveLine") {
            router.add_route(op, {"pose", "jointpos"}, [&](const wxz::core::CommandRouter::KvMap& kv) {
                ArmCommand c;
                c.kv = kv;
                c.op = wxz::core::KvCodec::get(kv, "op", "");
                c.id = wxz::core::KvCodec::get(kv, "id", "");
                set_out(fn(c, arm, logger));
            });
        } else if (op == "moveJoint") {
            router.add_route(op, {"jointpos"}, [&](const wxz::core::CommandRouter::KvMap& kv) {
                ArmCommand c;
                c.kv = kv;
                c.op = wxz::core::KvCodec::get(kv, "op", "");
                c.id = wxz::core::KvCodec::get(kv, "id", "");
                set_out(fn(c, arm, logger));
            });
        } else if (op == "path_download") {
            router.add_route(op, {"file"}, [&](const wxz::core::CommandRouter::KvMap& kv) {
                ArmCommand c;
                c.kv = kv;
                c.op = wxz::core::KvCodec::get(kv, "op", "");
                c.id = wxz::core::KvCodec::get(kv, "id", "");
                set_out(fn(c, arm, logger));
            });
        } else if (op == "demo_echo") {
            router.add_route(op, {"msg"}, [&](const wxz::core::CommandRouter::KvMap& kv) {
                ArmCommand c;
                c.kv = kv;
                c.op = wxz::core::KvCodec::get(kv, "op", "");
                c.id = wxz::core::KvCodec::get(kv, "id", "");
                set_out(fn(c, arm, logger));
            });
        } else if (op == "slowSpeed" || op == "quickStop") {
            router.add_route(op, {"enable"}, [&](const wxz::core::CommandRouter::KvMap& kv) {
                ArmCommand c;
                c.kv = kv;
                c.op = wxz::core::KvCodec::get(kv, "op", "");
                c.id = wxz::core::KvCodec::get(kv, "id", "");
                set_out(fn(c, arm, logger));
            });
        } else {
            router.add_route(op, {}, [&](const wxz::core::CommandRouter::KvMap& kv) {
                ArmCommand c;
                c.kv = kv;
                c.op = wxz::core::KvCodec::get(kv, "op", "");
                c.id = wxz::core::KvCodec::get(kv, "id", "");
                set_out(fn(c, arm, logger));
            });
        }
    }

    router.dispatch_kv(cmd.kv);
    if (out_set) return out;

    // Fallback: shouldn't happen, but keep behavior safe.
    EventDTOUtil::KvMap resp = make_base_resp(cmd);
    logger.log(LogLevel::Warn, "dispatch produced no response");
    arm_set_error(resp, ArmErrc::UnknownOp, "unknown_op");
    return resp;
}

} // namespace wxz::workstation::arm_control::internal
