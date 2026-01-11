#include "wxz_workstation/arm_control/internal/arm_control_internal.h"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include "service_common.h"

#include "dto/event_dto_cdr.h"

namespace wxz::workstation::arm_control::internal {

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

bool load_sdk(SdkApi& api, const Logger& logger) {
#if defined(WXZ_ARM_LINK_SDK) && (WXZ_ARM_LINK_SDK)
    (void)logger;
    api.handle = nullptr;
    api.cr_create_robot = &cr_create_robot;
    api.cr_destroy_robot = &cr_destroy_robot;
    api.cr_move_line = &cr_move_line;
    api.cr_move_joint = &cr_move_joint;
    api.cr_get_robotMode = &cr_get_robotMode;
    api.cr_get_robotMoveStatus = &cr_get_robotMoveStatus;
    api.cr_poweron = &cr_poweron;
    api.cr_enable = &cr_enable;
    api.cr_stop = &cr_stop;
    api.cr_FaultReset = &cr_FaultReset;
    api.cr_set_configDigitalOut = &cr_set_configDigitalOut;
    api.cr_get_configDigitalIn = &cr_get_configDigitalIn;
    api.cr_path_file2pathData = &cr_path_file2pathData;
    api.cr_path_download = &cr_path_download;
    api.cr_path_action = &cr_path_action;
    api.cr_path_currentRunStatus_get = &cr_path_currentRunStatus_get;
    api.cr_path_all_index_get = &cr_path_all_index_get;
    api.cr_get_jointActualPos = &cr_get_jointActualPos;
    return true;
#else
#error "Direct-link SDK required; build with -DWXZ_ARM_LINK_SDK=ON"
#endif
}

ArmSdkClient::ArmSdkClient(ArmConn conn, const SdkApi* api) : conn_(std::move(conn)), api_(api) {}

ArmSdkClient::~ArmSdkClient() {
    disconnect();
}

CRresult ArmSdkClient::connect() {
    if (connected_) return success;
    RobotHandle handle{};
    if (!api_ || !api_->cr_create_robot) return CR_FAILED;
    const CRresult r = api_->cr_create_robot(&handle, conn_.ip.c_str(), conn_.port, conn_.passwd.c_str());
    if (r == success) {
        handle_ = handle;
        connected_ = true;
    }
    return r;
}

void ArmSdkClient::disconnect() {
    if (!connected_) return;
    if (api_ && api_->cr_destroy_robot) (void)api_->cr_destroy_robot(handle_);
    connected_ = false;
    handle_ = 0;
}

CRresult ArmSdkClient::ensure_connected() {
    CRresult r = connect();
    if (r == success) return r;
    disconnect();
    r = connect();
    return r;
}

std::optional<bool> ArmSdkClient::read_config_di(int index) {
    const CRresult cr = ensure_connected();
    if (cr != success) return std::nullopt;
    if (!api_ || !api_->cr_get_configDigitalIn) return std::nullopt;
    BOOL val = FALSE;
    const CRresult r = api_->cr_get_configDigitalIn(handle_, index, &val);
    if (r != success) {
        disconnect();
        return std::nullopt;
    }
    return (val == TRUE);
}

std::optional<PathRunMsg> ArmSdkClient::get_path_run_status() {
    const CRresult cr = ensure_connected();
    if (cr != success) return std::nullopt;
    if (!api_ || !api_->cr_path_currentRunStatus_get) return std::nullopt;
    PathRunMsg msg{};
    const CRresult r = api_->cr_path_currentRunStatus_get(handle_, &msg);
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
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;
    if (!api_ || !api_->cr_get_jointActualPos) return CR_FAILED;

#if defined(ROB_AXIS_NUM) && (ROB_AXIS_NUM < 6)
    return CR_FAILED;
#else
    double pos[ROB_AXIS_NUM]{};
    const CRresult r = api_->cr_get_jointActualPos(handle_, pos);
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
    // SDK: pathrunstatus 1=running; 0 or 10001=stopped.
    return st->pathrunstatus != 1;
}

bool ArmSdkClient::IsAllTrajectoriesComplete() {
    // Current SDK reports the current path execution state; controller executes one at a time.
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
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;
    if (!api_ || !api_->cr_path_action) return CR_FAILED;

    const int path_index = Env::get_int("WXZ_ARM_PATH_INDEX", 0);
    logger.log(LogLevel::Info, std::string("ExecuteTrajectory path_index=") + std::to_string(path_index));
    CRresult r = api_->cr_path_action(handle_, path_index, 1 /*start*/);
    if (r != success) {
        disconnect();
        return r;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (IsStopSignal()) {
            (void)api_->cr_path_action(handle_, path_index, 0 /*stop*/);
            return CR_FAILED;
        }
        if (IsTrajectoryComplete()) return success;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    (void)api_->cr_path_action(handle_, path_index, 0 /*stop*/);
    return CR_FAILED;
}

CRresult ArmSdkClient::EmergencyStop(Logger const& logger) {
    (void)logger;
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;
    if (!api_ || !api_->cr_stop) return CR_FAILED;
    const CRresult r = api_->cr_stop(handle_);
    if (r != success) disconnect();
    return r;
}

CRresult ArmSdkClient::ResetSystem(Logger const& logger) {
    (void)logger;
    return fault_reset();
}

CRresult ArmSdkClient::moveL(const std::array<double, 6>& jointpos,
                            const std::array<double, 6>& pose,
                            double speed,
                            double acc,
                            double jerk) {
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;

    PointControlPara p{};
    for (int i = 0; i < ROB_AXIS_NUM; ++i) {
        const std::size_t idx = static_cast<std::size_t>(i);
        const bool is_angle = (i >= 3);
        p.pose[idx] = is_angle ? (pose[idx] * 180.0 / 3.14159265358979323846) : pose[idx];
        p.jointpos[idx] = jointpos[idx] * 180.0 / 3.14159265358979323846;
        p.tcpOffset[idx] = 0;
        p.coordinatePose[idx] = 0;
        p.speed[idx] = speed;
        p.acc[idx] = acc;
        p.jerk[idx] = jerk;
    }
    p.tcpID = -1;
    p.coordinateType = baseCoordinate;
    p.pointTransType = pointTransStop;
    p.pointTransRadius = 30;
    p.poseTranType = poseTranMoveToTargetPose;
    p.motiontriggerMode = MovetriggerbyOnlyRpc;

    if (!api_ || !api_->cr_move_line) return CR_FAILED;
    const CRresult r = api_->cr_move_line(handle_, p, TRUE);
    if (r != success) {
        disconnect();
    }
    return r;
}

CRresult ArmSdkClient::moveJ(const std::array<double, 6>& jointpos, double speed_rad_per_s) {
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
        p.jerk[idx] = speed_deg * 3;
    }
    p.tcpID = -1;
    p.coordinateType = jointCoordinate;
    p.pointTransType = pointTransStop;
    p.pointTransRadius = 30;
    p.poseTranType = poseTranMoveToTargetPose;
    p.motiontriggerMode = MovetriggerbyOnlyRpc;

    if (!api_ || !api_->cr_move_joint) return CR_FAILED;
    const CRresult r = api_->cr_move_joint(handle_, p, TRUE);
    if (r != success) {
        disconnect();
    }
    return r;
}

CRresult ArmSdkClient::power_on_enable(Logger const& logger) {
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;

    enum RobotModes mode = Closed;
    if (!api_ || !api_->cr_get_robotMode) return CR_FAILED;
    CRresult r = api_->cr_get_robotMode(handle_, &mode);
    if (r != success) return r;
    logger.log(LogLevel::Debug, std::string("robotMode=") + std::to_string(static_cast<int>(mode)));

    if (mode == JointPowerOff) {
        if (!api_->cr_poweron) return CR_FAILED;
        r = api_->cr_poweron(handle_);
        if (r != success) return r;

        for (int i = 0; i < 40; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            r = api_->cr_get_robotMode(handle_, &mode);
            if (r != success) return r;
            if (mode == JointIdle) break;
        }

        if (!api_->cr_enable) return CR_FAILED;
        r = api_->cr_enable(handle_);
        if (r != success) return r;

        for (int i = 0; i < 40; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            r = api_->cr_get_robotMode(handle_, &mode);
            if (r != success) return r;
            if (mode == ProgramStop) break;
        }
    }

    return success;
}

CRresult ArmSdkClient::get_robot_mode(int& out_mode) {
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;
    enum RobotModes mode = Closed;
    if (!api_ || !api_->cr_get_robotMode) return CR_FAILED;
    const CRresult r = api_->cr_get_robotMode(handle_, &mode);
    if (r != success) {
        disconnect();
        return r;
    }
    out_mode = static_cast<int>(mode);
    return success;
}

CRresult ArmSdkClient::fault_reset() {
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;
    if (!api_ || !api_->cr_FaultReset) return CR_FAILED;
    return api_->cr_FaultReset(handle_);
}

CRresult ArmSdkClient::slow_speed(bool enable) {
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;
    if (!api_ || !api_->cr_set_configDigitalOut) return CR_FAILED;
    return api_->cr_set_configDigitalOut(handle_, 0, enable ? TRUE : FALSE);
}

CRresult ArmSdkClient::quick_stop(bool enable) {
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;
    if (!api_ || !api_->cr_set_configDigitalOut) return CR_FAILED;
    return api_->cr_set_configDigitalOut(handle_, 1, enable ? FALSE : TRUE);
}

CRresult ArmSdkClient::path_download(const std::string& file,
                                    int index,
                                    int move_type,
                                    std::size_t max_points) {
    const CRresult cr = ensure_connected();
    if (cr != success) return cr;

    PathData pathData{};
    pathData.pathPoints = new PathPoint[max_points];

    char path_buf[1024];
    std::snprintf(path_buf, sizeof(path_buf), "%s", file.c_str());

    if (!api_ || !api_->cr_path_file2pathData) {
        delete[] pathData.pathPoints;
        return CR_FAILED;
    }
    CRresult r = api_->cr_path_file2pathData(path_buf, &pathData);
    if (r != success) {
        delete[] pathData.pathPoints;
        disconnect();
        return r;
    }

    PathDownloadData dl{};
    dl.pathData = pathData;
    dl.pathPara.index = index;
    dl.pathPara.moveType = move_type;
    if (!api_->cr_path_download) {
        delete[] pathData.pathPoints;
        return CR_FAILED;
    }
    r = api_->cr_path_download(handle_, dl);

    delete[] pathData.pathPoints;

    if (r != success) {
        disconnect();
    }
    return r;
}

} // namespace wxz::workstation::arm_control::internal
