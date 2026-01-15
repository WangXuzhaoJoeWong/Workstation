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

// arm_control 通过直接链接依赖 SDK（不使用 dlopen/dlsym）。
// 若未开启该模式，则直接编译失败，避免出现“运行时才发现 SDK 不可用”的不确定性。
#if !defined(WXZ_ARM_LINK_SDK) || !(WXZ_ARM_LINK_SDK)
#error "Direct-link SDK required; build with -DWXZ_ARM_LINK_SDK=ON"
#endif

namespace wxz::workstation::arm_control::internal {

// 保持 LogLevel/Logger 与既有输出一致。
using LogLevel = wxz::core::LogLevel;

using Logger = wxz::core::Logger;

struct Env {
    /// 读取字符串环境变量；若不存在则返回 def。
    static std::string get_str(const char* key, const std::string& def);

    /// 读取布尔环境变量；若不存在则返回 def。
    static bool get_bool(const char* key, bool def);

    /// 读取整型环境变量；若不存在则返回 def。
    static int get_int(const char* key, int def);

    /// 读取 size_t 环境变量；若不存在则返回 def。
    static std::size_t get_size(const char* key, std::size_t def);
};

/// 解析形如 "a,b,c,d,e,f" 的 6 维 CSV。
std::optional<std::array<double, 6>> parse_csv6(const std::string& s);

/// 解析 double；失败返回 std::nullopt。
std::optional<double> parse_double(const std::string& s);

/// 解析 int；失败返回 std::nullopt。
std::optional<int> parse_int(const std::string& s);

/// 解析 size_t；失败返回 std::nullopt。
std::optional<std::size_t> parse_size(const std::string& s);

struct Cmd {
    std::string raw;
};

template <class T>
class MpscQueue {
public:
    void push(T v) {
        std::lock_guard<std::mutex> lock(mu_);
        q_.push(std::move(v));
    }

    bool try_pop(T& out) {
        std::lock_guard<std::mutex> lock(mu_);
        if (q_.empty()) return false;
        out = std::move(q_.front());
        q_.pop();
        return true;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mu_);
        return q_.size();
    }

private:
    mutable std::mutex mu_;
    std::queue<T> q_;
};

class CmdQueue {
public:
    /// 创建一个带容量上限的队列（超限时 push 失败）。
    explicit CmdQueue(std::size_t max_size);

    /// 入队；当队列已满时返回 false。
    bool push(Cmd cmd);

    /// 尝试出队；队列为空时返回 std::nullopt。
    std::optional<Cmd> try_pop();

    /// 在 timeout 内等待出队；running 为 false 时提前返回。
    std::optional<Cmd> pop_for(std::chrono::milliseconds timeout, const std::atomic<bool>& running);

    /// 在 timeout 内等待出队；running() 为 false 时提前返回。
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
    /// 创建 /arm/status 发布器。
    StatusPublisher(int domain,
                    std::string topic,
                    std::string schema_id,
                    std::size_t dto_max_payload,
                    std::string dto_source);

    /// 发布一条 KV（内部会封装为 EventDTO）。
    void publish_kv(const EventDTOUtil::KvMap& kv);

    /// 默认 QoS（与系统其它组件保持一致）。
    static wxz::core::ChannelQoS default_qos();

    wxz::core::FastddsChannel chan;

private:
    std::string topic_;
    std::string schema_id_;
    std::string dto_source_;
};

constexpr CRresult CR_FAILED = static_cast<CRresult>(-1);

/// 机械臂客户端抽象接口（便于 mock/替换实现）。
class IArmClient {
public:
    virtual ~IArmClient() = default;

    /// 直线运动。
    virtual CRresult moveL(const std::array<double, 6>& jointpos,
                           const std::array<double, 6>& pose,
                           double speed,
                           double acc,
                           double jerk) = 0;

    /// 关节运动。
    virtual CRresult moveJ(const std::array<double, 6>& jointpos, double speed_rad_per_s) = 0;

    /// 上电并使能（实现可包含多步过程）。
    virtual CRresult power_on_enable(Logger const& logger) = 0;

    /// 查询当前 robot mode（值与 SDK enum RobotModes 对齐）。
    virtual CRresult get_robot_mode(int& out_mode) = 0;

    /// 清除故障。
    virtual CRresult fault_reset() = 0;

    /// 慢速模式开关。
    virtual CRresult slow_speed(bool enable) = 0;

    /// 急停开关。
    virtual CRresult quick_stop(bool enable) = 0;

    /// 下载轨迹文件。
    virtual CRresult path_download(const std::string& file, int index, int move_type, std::size_t max_points) = 0;
};

/// 基于 SDK 的机械臂客户端实现。
class ArmSdkClient final : public IArmClient {
public:
    /// 使用连接信息与已绑定的 SDK API 构造客户端。
    explicit ArmSdkClient(ArmConn conn);
    ~ArmSdkClient() override;

    // --- 高层辅助函数（行为/PLC 集成）---
    // 注意：DI 映射可通过环境变量配置：
    // - WXZ_ARM_START_DI_INDEX（默认：0）
    // - WXZ_ARM_STOP_DI_INDEX （默认：1）
    /// 机械臂是否就绪（综合判断）。
    bool IsArmReady();

    /// 机械臂是否已上电。
    bool IsPowerOn();

    /// 是否收到启动信号（DI）。
    bool IsStartSignal();

    /// 是否收到停止信号（DI）。
    bool IsStopSignal();

    /// 当前轨迹是否执行完成。
    bool IsTrajectoryComplete();

    /// 所有轨迹是否执行完成。
    bool IsAllTrajectoriesComplete();

    /// 初始化机械臂（连接/上电/使能等）。
    CRresult InitializeArm(Logger const& logger);

    /// 等待启动信号，最多等待 timeout。
    CRresult WaitForStart(std::chrono::milliseconds timeout, Logger const& logger);

    /// 执行轨迹，最多等待 timeout。
    CRresult ExecuteTrajectory(std::chrono::milliseconds timeout, Logger const& logger);

    /// 急停。
    CRresult EmergencyStop(Logger const& logger);

    /// 重置系统（用于故障恢复流程）。
    CRresult ResetSystem(Logger const& logger);

    // 查询实际关节角（SDK 单位：度）。
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
    /// 连接到机械臂控制器。
    CRresult connect();

    /// 断开连接（若已连接）。
    void disconnect();

    /// 确保处于已连接状态；必要时尝试重连。
    CRresult ensure_connected();

    /// 读取配置 DI，失败返回 std::nullopt。
    std::optional<bool> read_config_di(int index);

    /// 查询轨迹运行状态，失败返回 std::nullopt。
    std::optional<PathRunMsg> get_path_run_status();

    ArmConn conn_;
    RobotHandle handle_{0};
    bool connected_{false};

    // SDK handle + session 有状态且未声明线程安全。
    // 该服务存在多线程（DDS 回调 + 主循环），因此需要串行化所有 SDK 访问。
    mutable std::recursive_mutex sdk_mu_;
};

} // namespace wxz::workstation::arm_control::internal
