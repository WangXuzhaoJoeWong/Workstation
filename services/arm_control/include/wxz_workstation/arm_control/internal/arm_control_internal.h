#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>

#include "dto/event_dto.h"
#include "fastdds_channel.h"
#include "logger.h"

extern "C" {
#include "robotapi.h"
}

namespace wxz::workstation::arm_control::internal {

// Keep LogLevel/Logger consistent with existing output.
using LogLevel = wxz::core::LogLevel;

using Logger = wxz::core::Logger;

struct Env {
    static std::string get_str(const char* key, const std::string& def);
    static bool get_bool(const char* key, bool def);
    static int get_int(const char* key, int def);
    static std::size_t get_size(const char* key, std::size_t def);
};

std::optional<std::array<double, 6>> parse_csv6(const std::string& s);
std::optional<double> parse_double(const std::string& s);
std::optional<int> parse_int(const std::string& s);
std::optional<std::size_t> parse_size(const std::string& s);

struct Cmd {
    std::string raw;
};

class CmdQueue {
public:
    explicit CmdQueue(std::size_t max_size);

    bool push(Cmd cmd);
    std::optional<Cmd> pop_for(std::chrono::milliseconds timeout, const std::atomic<bool>& running);
    std::optional<Cmd> pop_for(std::chrono::milliseconds timeout, const std::function<bool()>& running);

private:
    std::mutex mu_;
    std::condition_variable cv_;
    std::queue<Cmd> q_;
    std::size_t max_size_{64};
};

struct ArmConn {
    std::string ip;
    int port{2323};
    std::string passwd;
};

struct StatusPublisher {
    StatusPublisher(int domain,
                    std::string topic,
                    std::string schema_id,
                    std::size_t dto_max_payload,
                    std::string dto_source);

    void publish_kv(const EventDTOUtil::KvMap& kv);
    static wxz::core::ChannelQoS default_qos();

    wxz::core::FastddsChannel chan;

private:
    std::string topic_;
    std::string schema_id_;
    std::string dto_source_;
};

// SDK API binding via direct link (no dlopen/dlsym).
struct SdkApi {
    void* handle{nullptr};
    CRresult (*cr_create_robot)(RobotHandle*, const char*, int, const char*){nullptr};
    CRresult (*cr_destroy_robot)(RobotHandle){nullptr};
    CRresult (*cr_move_line)(RobotHandle, PointControlPara, BOOL){nullptr};
    CRresult (*cr_move_joint)(RobotHandle, PointControlPara, BOOL){nullptr};
    CRresult (*cr_get_robotMode)(RobotHandle, enum RobotModes*){nullptr};
    CRresult (*cr_get_robotMoveStatus)(RobotHandle, BOOL*){nullptr};
    CRresult (*cr_poweron)(RobotHandle){nullptr};
    CRresult (*cr_enable)(RobotHandle){nullptr};
    CRresult (*cr_stop)(RobotHandle){nullptr};
    CRresult (*cr_FaultReset)(RobotHandle){nullptr};
    CRresult (*cr_set_configDigitalOut)(RobotHandle, int, BOOL){nullptr};
    CRresult (*cr_get_configDigitalIn)(RobotHandle, int, BOOL*){nullptr};
    CRresult (*cr_path_file2pathData)(char*, PathData*){nullptr};
    CRresult (*cr_path_download)(RobotHandle, PathDownloadData){nullptr};
    CRresult (*cr_path_action)(RobotHandle, int, int){nullptr};
    CRresult (*cr_path_currentRunStatus_get)(RobotHandle, PathRunMsg*){nullptr};
    CRresult (*cr_path_all_index_get)(RobotHandle, int*, int*){nullptr};
    CRresult (*cr_get_jointActualPos)(RobotHandle, double pos[ROB_AXIS_NUM]){nullptr};
};

constexpr CRresult CR_FAILED = static_cast<CRresult>(-1);

bool load_sdk(SdkApi& api, const Logger& logger);

class IArmClient {
public:
    virtual ~IArmClient() = default;

    virtual CRresult moveL(const std::array<double, 6>& jointpos,
                           const std::array<double, 6>& pose,
                           double speed,
                           double acc,
                           double jerk) = 0;

    virtual CRresult moveJ(const std::array<double, 6>& jointpos, double speed_rad_per_s) = 0;
    virtual CRresult power_on_enable(Logger const& logger) = 0;
    // Query current robot mode (value matches SDK enum RobotModes).
    virtual CRresult get_robot_mode(int& out_mode) = 0;
    virtual CRresult fault_reset() = 0;
    virtual CRresult slow_speed(bool enable) = 0;
    virtual CRresult quick_stop(bool enable) = 0;
    virtual CRresult path_download(const std::string& file, int index, int move_type, std::size_t max_points) = 0;
};

class ArmSdkClient final : public IArmClient {
public:
    explicit ArmSdkClient(ArmConn conn, const SdkApi* api);
    ~ArmSdkClient() override;

    // --- High-level helpers (behavior/PLC integration) ---
    // Note: DI mapping is configurable by env:
    // - WXZ_ARM_START_DI_INDEX (default: 0)
    // - WXZ_ARM_STOP_DI_INDEX  (default: 1)
    bool IsArmReady();
    bool IsPowerOn();
    bool IsStartSignal();
    bool IsStopSignal();
    bool IsTrajectoryComplete();
    bool IsAllTrajectoriesComplete();

    CRresult InitializeArm(Logger const& logger);
    CRresult WaitForStart(std::chrono::milliseconds timeout, Logger const& logger);
    CRresult ExecuteTrajectory(std::chrono::milliseconds timeout, Logger const& logger);
    CRresult EmergencyStop(Logger const& logger);
    CRresult ResetSystem(Logger const& logger);

    // Query actual joint position (SDK unit: degrees).
    CRresult GetJointActualPosDeg(std::array<double, 6>& out_deg);

    CRresult moveL(const std::array<double, 6>& jointpos,
                   const std::array<double, 6>& pose,
                   double speed,
                   double acc,
                   double jerk) override;

    CRresult moveJ(const std::array<double, 6>& jointpos, double speed_rad_per_s) override;
    CRresult power_on_enable(Logger const& logger) override;
    CRresult get_robot_mode(int& out_mode) override;
    CRresult fault_reset() override;
    CRresult slow_speed(bool enable) override;
    CRresult quick_stop(bool enable) override;
    CRresult path_download(const std::string& file, int index, int move_type, std::size_t max_points) override;

private:
    CRresult connect();
    void disconnect();
    CRresult ensure_connected();

    std::optional<bool> read_config_di(int index);
    std::optional<PathRunMsg> get_path_run_status();

    ArmConn conn_;
    RobotHandle handle_{0};
    bool connected_{false};
    const SdkApi* api_{nullptr};
};

} // namespace wxz::workstation::arm_control::internal
