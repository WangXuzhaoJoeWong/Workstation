#include "internal/arm_control_internal.h"

#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#include "service_common.h"

#include "dto/event_dto_cdr.h"

namespace wxz::workstation::arm_control::internal {

namespace {

const char* cr_result_name(CRresult r) {
    switch (r) {
        case success: return "success";
        case error: return "error";
        case thread_running: return "thread_running";
        case operate_timeout: return "operate_timeout";
        case result_invalid: return "result_invalid";
        case out_of_range: return "out_of_range";
        case mutex_invalid: return "mutex_invalid";
        case para_error: return "para_error";
        case no_result: return "no_result";
        case no_assignTCPindex: return "no_assignTCPindex";
        case no_handle: return "no_handle";
        case handle_repeat: return "handle_repeat";
        case repeat_name: return "repeat_name";
        case delete_invalid: return "delete_invalid";
        case set_bit_reg_invalid: return "set_bit_reg_invalid";
        case repeat_id: return "repeat_id";
        case file_encryption: return "file_encryption";
        case robotmode_error: return "robotmode_error";
        case move_error: return "move_error";
        default: return "unknown";
    }
}

bool should_disconnect_on_error(CRresult r) {
    // 经验规则：仅在疑似传输/会话问题时断开连接。
    // 运动/状态类错误尽量保持连接，避免反复重连刷屏。
    return (r == operate_timeout || r == thread_running);
}

} // namespace

std::string Env::get_str(const char* key, const std::string& def) {
    return wxz::core::getenv_str(key, def);
}

bool Env::get_bool(const char* key, bool def) {
    const char* v = std::getenv(key);
    if (!v || !*v) return def;
    if (*v == '1') return true;
    if (*v == '0') return false;
    return (std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0);
}

int Env::get_int(const char* key, int def) {
    return wxz::core::getenv_int(key, def);
}

std::size_t Env::get_size(const char* key, std::size_t def) {
    const char* v = std::getenv(key);
    if (!v || !*v) return def;
    try {
        return static_cast<std::size_t>(std::stoul(v));
    } catch (...) {
        return def;
    }
}

std::optional<std::array<double, 6>> parse_csv6(const std::string& s) {
    std::array<double, 6> out{};
    std::size_t start = 0;
    int idx = 0;
    while (idx < 6) {
        std::size_t end = s.find(',', start);
        std::string token = (end == std::string::npos) ? s.substr(start) : s.substr(start, end - start);
        while (!token.empty() && token.front() == ' ') token.erase(token.begin());
        while (!token.empty() && token.back() == ' ') token.pop_back();
        if (token.empty()) return std::nullopt;
        try {
            out[static_cast<std::size_t>(idx)] = std::stod(token);
        } catch (...) {
            return std::nullopt;
        }
        ++idx;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (idx != 6) return std::nullopt;
    return out;
}

std::optional<double> parse_double(const std::string& s) {
    try {
        std::size_t idx = 0;
        double v = std::stod(s, &idx);
        if (idx != s.size()) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> parse_int(const std::string& s) {
    try {
        std::size_t idx = 0;
        int v = std::stoi(s, &idx);
        if (idx != s.size()) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::size_t> parse_size(const std::string& s) {
    try {
        std::size_t idx = 0;
        auto v = static_cast<std::size_t>(std::stoul(s, &idx));
        if (idx != s.size()) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

CmdQueue::CmdQueue(std::size_t max_size) : max_size_(max_size) {}

bool CmdQueue::push(Cmd cmd) {
    std::lock_guard<std::mutex> lock(mu_);
    if (q_.size() >= max_size_) return false;
    q_.push(std::move(cmd));
    cv_.notify_one();
    return true;
}

std::optional<Cmd> CmdQueue::try_pop() {
    std::lock_guard<std::mutex> lock(mu_);
    if (q_.empty()) return std::nullopt;
    Cmd cmd = std::move(q_.front());
    q_.pop();
    return cmd;
}

std::optional<Cmd> CmdQueue::pop_for(std::chrono::milliseconds timeout, const std::atomic<bool>& running) {
    return pop_for(timeout, [&] { return running.load(); });
}

std::optional<Cmd> CmdQueue::pop_for(std::chrono::milliseconds timeout, const std::function<bool()>& running) {
    std::unique_lock<std::mutex> lock(mu_);
    if (!cv_.wait_for(lock, timeout, [&] { return !q_.empty() || !running(); })) {
        return std::nullopt;
    }
    if (q_.empty()) return std::nullopt;
    Cmd cmd = std::move(q_.front());
    q_.pop();
    return cmd;
}

StatusPublisher::StatusPublisher(int domain,
                                 std::string topic,
                                 std::string schema_id,
                                 std::size_t dto_max_payload,
                                 std::string dto_source)
    : chan(domain, topic, default_qos(), dto_max_payload),
      topic_(std::move(topic)),
      schema_id_(std::move(schema_id)),
      dto_source_(std::move(dto_source)) {}

void StatusPublisher::publish_kv(const EventDTOUtil::KvMap& kv) {
    const std::string msg = EventDTOUtil::buildPayloadKv(kv);

    ::EventDTO dto;
    dto.version = 1;
    dto.schema_id = schema_id_;
    dto.topic = topic_;
    dto.payload = msg;
    EventDTOUtil::fillMeta(dto, dto_source_);
    if (auto it = kv.find("id"); it != kv.end() && !it->second.empty()) {
        dto.event_id = it->second;
    }

    std::vector<std::uint8_t> buf;
    if (!wxz::dto::encode_event_dto_cdr(dto, buf)) {
        return;
    }
    chan.publish(buf.data(), buf.size());
}

wxz::core::ChannelQoS StatusPublisher::default_qos() {
    wxz::core::ChannelQoS qos = wxz::core::default_reliable_qos();
    qos.transport_priority = 32;
    return qos;
}

ArmSdkClient::ArmSdkClient(ArmConn conn) : conn_(std::move(conn)) {}

ArmSdkClient::~ArmSdkClient() {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    disconnect();
}

CRresult ArmSdkClient::connect() {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    if (connected_) return success;
    RobotHandle handle{};
    const CRresult r = ::cr_create_robot(&handle, conn_.ip.c_str(), conn_.port, conn_.passwd.c_str());
    if (r == success) {
        handle_ = handle;
        connected_ = true;
    }
    return r;
}

void ArmSdkClient::disconnect() {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    if (!connected_) return;
    (void)::cr_destroy_robot(handle_);
    connected_ = false;
    handle_ = 0;
}

CRresult ArmSdkClient::ensure_connected() {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    CRresult r = connect();
    if (r == success) return r;
    disconnect();
    r = connect();
    return r;
}

std::optional<bool> ArmSdkClient::read_config_di(int index) {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    const CRresult cr = ensure_connected();
    if (cr != success) return std::nullopt;
    BOOL val = FALSE;
    const CRresult r = ::cr_get_configDigitalIn(handle_, index, &val);
    if (r != success) {
        disconnect();
        return std::nullopt;
    }
    return (val == TRUE);
}

std::optional<PathRunMsg> ArmSdkClient::get_path_run_status() {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    const CRresult cr = ensure_connected();
    if (cr != success) return std::nullopt;
    PathRunMsg msg{};
    const CRresult r = ::cr_path_currentRunStatus_get(handle_, &msg);
    if (r != success) {
        disconnect();
        return std::nullopt;
    }
    return msg;
}

bool ArmSdkClient::IsArmReady() {
    int mode = static_cast<int>(Closed);
    const CRresult r = get_robot_mode(mode);
    if (r != success) return false;
    return (mode == static_cast<int>(ProgramStop) || mode == static_cast<int>(Jog) || mode == static_cast<int>(JointIdle));
}

bool ArmSdkClient::IsPowerOn() {
    int mode = static_cast<int>(Closed);
    const CRresult r = get_robot_mode(mode);
    if (r != success) return false;
    return (mode != static_cast<int>(JointPowerOff) && mode != static_cast<int>(Closed));
}

bool ArmSdkClient::IsStartSignal() {
    const int idx = Env::get_int("WXZ_ARM_START_DI_INDEX", 0);
    const auto v = read_config_di(idx);
    return v.value_or(false);
}

bool ArmSdkClient::IsStopSignal() {
    const int idx = Env::get_int("WXZ_ARM_STOP_DI_INDEX", 1);
    const auto v = read_config_di(idx);
    return v.value_or(false);
}

CRresult ArmSdkClient::GetJointActualPosDeg(std::array<double, 6>& out_deg) {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;

#if defined(ROB_AXIS_NUM) && (ROB_AXIS_NUM < 6)
    return CR_FAILED;
#else
    double pos[ROB_AXIS_NUM]{};
    const CRresult r = ::cr_get_jointActualPos(handle_, pos);
    if (r != success) {
        disconnect();
        return r;
    }
    for (std::size_t i = 0; i < 6; ++i) out_deg[i] = pos[i];
    return r;
#endif
}

bool ArmSdkClient::IsTrajectoryComplete() {
    const auto st = get_path_run_status();
    if (!st) return false;
    // SDK：pathrunstatus 1=running；0 或 10001=stopped。
    return st->pathrunstatus != 1;
}

bool ArmSdkClient::IsAllTrajectoriesComplete() {
    // 当前 SDK 只上报“当前路径”的执行状态；控制器一次只执行一条路径。
    return IsTrajectoryComplete();
}

CRresult ArmSdkClient::InitializeArm(Logger const& logger) {
    return power_on_enable(logger);
}

CRresult ArmSdkClient::WaitForStart(std::chrono::milliseconds timeout, Logger const& logger) {
    (void)logger;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (IsStopSignal()) return CR_FAILED;
        if (IsStartSignal()) return success;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return CR_FAILED;
}

CRresult ArmSdkClient::ExecuteTrajectory(std::chrono::milliseconds timeout, Logger const& logger) {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;

    const int path_index = Env::get_int("WXZ_ARM_PATH_INDEX", 0);
    logger.log(LogLevel::Info, std::string("ExecuteTrajectory path_index=") + std::to_string(path_index));
    CRresult r = ::cr_path_action(handle_, path_index, 1 /*start*/);
    if (r != success) {
        disconnect();
        return r;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (IsStopSignal()) {
            (void)::cr_path_action(handle_, path_index, 0 /*stop*/);
            return CR_FAILED;
        }
        if (IsTrajectoryComplete()) return success;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    (void)::cr_path_action(handle_, path_index, 0 /*stop*/);
    return CR_FAILED;
}

CRresult ArmSdkClient::EmergencyStop(Logger const& logger) {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    (void)logger;
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;
    const CRresult r = ::cr_stop(handle_);
    if (r != success) disconnect();
    return r;
}

CRresult ArmSdkClient::ResetSystem(Logger const& logger) {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    (void)logger;
    return fault_reset();
}

CRresult ArmSdkClient::moveL(const std::array<double, 6>& jointpos,
                            const std::array<double, 6>& pose,
                            double speed,
                            double acc,
                            double jerk) {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;

    // 最后一层安全检查（不完全依赖上游校验）。
    auto is_finite6 = [](const std::array<double, 6>& v) {
        for (double x : v) {
            if (!std::isfinite(x)) return false;
        }
        return true;
    };
    if (!is_finite6(jointpos) || !is_finite6(pose) || !std::isfinite(speed) || !std::isfinite(acc) || !std::isfinite(jerk)) {
        std::cerr << "moveL rejected: non-finite inputs" << "\n";
        disconnect();
        return CR_FAILED;
    }

    // 单位约定：pose xyz 为 mm，pose rpy 为 rad；jointpos 为 rad；speed 为 mm/s。
    // 防止常见且灾难性的错误：把“角度”当成“弧度”传入。
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kMaxAbsAngleRadSuspicious = 10.0; // 约 572 度
    for (int i = 3; i < 6; ++i) {
        if (std::fabs(pose[static_cast<std::size_t>(i)]) > kMaxAbsAngleRadSuspicious) {
            // 默认拒绝；高级用户可通过环境变量显式放行。
            if (!Env::get_bool("WXZ_ARM_ALLOW_LARGE_ANGLE", false)) {
                std::cerr << "moveL rejected: pose angle(rad) suspicious (>" << kMaxAbsAngleRadSuspicious
                          << "), set WXZ_ARM_ALLOW_LARGE_ANGLE=1 to override" << "\n";
                disconnect();
                return CR_FAILED;
            }
            break;
        }
    }
    for (int i = 0; i < 6; ++i) {
        if (std::fabs(jointpos[static_cast<std::size_t>(i)]) > kMaxAbsAngleRadSuspicious) {
            if (!Env::get_bool("WXZ_ARM_ALLOW_LARGE_JOINT", false)) {
                std::cerr << "moveL rejected: jointpos(rad) suspicious (>" << kMaxAbsAngleRadSuspicious
                          << "), set WXZ_ARM_ALLOW_LARGE_JOINT=1 to override" << "\n";
                disconnect();
                return CR_FAILED;
            }
            break;
        }
    }

    if (speed <= 0.0 || speed > 3000.0) {
        std::cerr << "moveL rejected: speed out of range: " << speed << "\n";
        disconnect();
        return CR_FAILED;
    }
    if (acc < 0.0 || acc > 20000.0 || jerk < 0.0 || jerk > 20000.0) {
        std::cerr << "moveL rejected: acc/jerk out of range: acc=" << acc << " jerk=" << jerk << "\n";
        disconnect();
        return CR_FAILED;
    }

    PointControlPara p{};
    for (int i = 0; i < ROB_AXIS_NUM; ++i) {
        const std::size_t idx = static_cast<std::size_t>(i);
        const bool is_angle = (i >= 3);
        p.pose[idx] = is_angle ? (pose[idx] * 180.0 / kPi) : pose[idx];
        p.jointpos[idx] = jointpos[idx] * 180.0 / kPi;
        p.tcpOffset[idx] = 0;
        p.coordinatePose[idx] = 0;
        p.speed[idx] = speed;
        p.acc[idx] = acc;
        // 注意：SDK 将 jerk 标记为保留参数；保持为 0 可避免
        // 固件因为“保留字段非零”而拒绝（常见 result_invalid 诱因）。
        (void)jerk;
        p.jerk[idx] = 0;
    }
    p.tcpID = -1;
    p.coordinateType = baseCoordinate;
    p.pointTransType = pointTransStop;
    // pointTransRadius 同样被标记为保留字段；stop 场景保持为 0。
    p.pointTransRadius = 0;
    p.poseTranType = poseTranMoveToTargetPose;
    p.motiontriggerMode = MovetriggerbyOnlyRpc;

    // 可选 dry-run：仅打印计算后的参数，不实际下发运动。
    if (Env::get_bool("WXZ_ARM_DRY_RUN", false)) {
        std::ostringstream os;
        os.setf(std::ios::fixed);
        os << "moveL dry_run: "
           << "pose_mm_rad=[" << pose[0] << "," << pose[1] << "," << pose[2] << "," << pose[3] << "," << pose[4]
           << "," << pose[5] << "] "
           << "pose_deg=[" << p.pose[3] << "," << p.pose[4] << "," << p.pose[5] << "] "
           << "joint_rad=[" << jointpos[0] << "," << jointpos[1] << "," << jointpos[2] << "," << jointpos[3] << ","
           << jointpos[4] << "," << jointpos[5] << "] "
           << "joint_deg=[" << p.jointpos[0] << "," << p.jointpos[1] << "," << p.jointpos[2] << "," << p.jointpos[3]
           << "," << p.jointpos[4] << "," << p.jointpos[5] << "] "
           << "speed=" << speed << " acc=" << acc << " jerk=" << jerk;
        std::cerr << os.str() << "\n";
        return success;
    }

    const CRresult r = ::cr_move_line(handle_, p, TRUE);
    if (r != success) {
        // 增补诊断信息，便于区分：参数/单位问题 vs robot mode 问题 vs 运动状态问题。
        enum RobotModes mode = Closed;
        BOOL moving = FALSE;
        (void)::cr_get_robotMode(handle_, &mode);
        (void)::cr_get_robotMoveStatus(handle_, &moving);
        std::cerr << "moveL failed code=" << static_cast<int>(r) << " (" << cr_result_name(r) << ")"
              << " robotMode=" << static_cast<int>(mode)
              << " isMoving=" << (moving == TRUE ? 1 : 0)
              << " speed=" << speed << " acc=" << acc << " jerk_in=" << jerk
              << " coordinateType=" << static_cast<int>(p.coordinateType)
              << " tcpID=" << p.tcpID
              << " pointTransType=" << static_cast<int>(p.pointTransType)
              << " motiontriggerMode=" << static_cast<int>(p.motiontriggerMode)
              << " xyz_mm=[" << p.pose[0] << "," << p.pose[1] << "," << p.pose[2] << "]"
              << " rpy_deg=[" << p.pose[3] << "," << p.pose[4] << "," << p.pose[5] << "]"
              << " joint_deg=[" << p.jointpos[0] << "," << p.jointpos[1] << "," << p.jointpos[2] << ","
              << p.jointpos[3] << "," << p.jointpos[4] << "," << p.jointpos[5] << "]"
              << "\n";

        if (should_disconnect_on_error(r)) {
            disconnect();
        }
    }
    return r;
}

CRresult ArmSdkClient::moveJ(const std::array<double, 6>& jointpos, double speed_rad_per_s) {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;

    const double speed_deg = speed_rad_per_s * 180.0 / 3.14159265358979323846;
    PointControlPara p{};
    for (int i = 0; i < ROB_AXIS_NUM; ++i) {
        const std::size_t idx = static_cast<std::size_t>(i);
        p.pose[idx] = 0;
        p.jointpos[idx] = jointpos[idx] * 180.0 / 3.14159265358979323846;
        p.tcpOffset[idx] = 0;
        p.coordinatePose[idx] = 0;
        p.speed[idx] = speed_deg;
        p.acc[idx] = speed_deg * 3;
        // SDK 保留字段；保持为 0 避免 result_invalid。
        p.jerk[idx] = 0;
    }
    p.tcpID = -1;
    p.coordinateType = jointCoordinate;
    p.pointTransType = pointTransStop;
    p.pointTransRadius = 0;
    p.poseTranType = poseTranMoveToTargetPose;
    p.motiontriggerMode = MovetriggerbyOnlyRpc;

    const CRresult r = ::cr_move_joint(handle_, p, TRUE);
    if (r != success) {
        enum RobotModes mode = Closed;
        BOOL moving = FALSE;
        (void)::cr_get_robotMode(handle_, &mode);
        (void)::cr_get_robotMoveStatus(handle_, &moving);
        std::cerr << "moveJ failed code=" << static_cast<int>(r) << " (" << cr_result_name(r) << ")"
                  << " robotMode=" << static_cast<int>(mode)
                  << " isMoving=" << (moving == TRUE ? 1 : 0)
                  << " speed_rad_per_s=" << speed_rad_per_s
                  << " coordinateType=" << static_cast<int>(p.coordinateType)
                  << "\n";

        if (should_disconnect_on_error(r)) {
            disconnect();
        }
    }
    return r;
}

CRresult ArmSdkClient::power_on_enable(Logger const& logger) {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;

    enum RobotModes mode = Closed;
    CRresult r = ::cr_get_robotMode(handle_, &mode);
    if (r != success) return r;
    logger.log(LogLevel::Debug, std::string("robotMode=") + std::to_string(static_cast<int>(mode)));

    if (mode == JointPowerOff) {
        r = ::cr_poweron(handle_);
        if (r != success) return r;

        for (int i = 0; i < 40; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            r = ::cr_get_robotMode(handle_, &mode);
            if (r != success) return r;
            if (mode == JointIdle) break;
        }

        r = ::cr_enable(handle_);
        if (r != success) return r;

        for (int i = 0; i < 40; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            r = ::cr_get_robotMode(handle_, &mode);
            if (r != success) return r;
            if (mode == ProgramStop) break;
        }
    }

    return success;
}

CRresult ArmSdkClient::get_robot_mode(int& out_mode) {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;
    enum RobotModes mode = Closed;
    const CRresult r = ::cr_get_robotMode(handle_, &mode);
    if (r != success) {
        disconnect();
        return r;
    }
    out_mode = static_cast<int>(mode);
    return success;
}

CRresult ArmSdkClient::fault_reset() {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;
    return ::cr_FaultReset(handle_);
}

CRresult ArmSdkClient::slow_speed(bool enable) {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;
    return ::cr_set_configDigitalOut(handle_, 0, enable ? TRUE : FALSE);
}

CRresult ArmSdkClient::quick_stop(bool enable) {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;
    return ::cr_set_configDigitalOut(handle_, 1, enable ? FALSE : TRUE);
}

CRresult ArmSdkClient::path_download(const std::string& file,
                                    int index,
                                    int move_type,
                                    std::size_t max_points) {
    std::lock_guard<std::recursive_mutex> lock(sdk_mu_);
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;

    PathData pathData{};
    pathData.pathPoints = new PathPoint[max_points];

    char path_buf[1024];
    std::snprintf(path_buf, sizeof(path_buf), "%s", file.c_str());

    CRresult r = ::cr_path_file2pathData(path_buf, &pathData);
    if (r != success) {
        delete[] pathData.pathPoints;
        disconnect();
        return r;
    }

    PathDownloadData dl{};
    dl.pathData = pathData;
    dl.pathPara.index = index;
    dl.pathPara.moveType = move_type;
    r = ::cr_path_download(handle_, dl);

    delete[] pathData.pathPoints;

    if (r != success) {
        disconnect();
    }
    return r;
}

} // namespace wxz::workstation::arm_control::internal
