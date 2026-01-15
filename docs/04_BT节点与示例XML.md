# BT 节点清单与示例 XML

本文档聚焦 `workstation_bt_service` 注册的 BT 节点，以及如何在本仓库里快速找到可用的示例 XML。

## 1) BT 节点清单（bt_service 内置）

> 以 `Workstation/services/bt_service/src/arm_nodes.cpp` 的注册为准。

### 1.1 动作（Action）

- `ArmPowerOn` / `PowerOn` / `power_on_enable` / `InitializeArm`
  - 含义：上电+使能（对应 arm_control 的 `power_on_enable`）。

- `ArmMoveL` / `MoveL` / `moveL`
  - 端口：
    - `pose`（必填）
    - `jointpos`（必填）
    - `speed`（默认 30）
    - `acc`（默认 30）
    - `jerk`（默认 60）

- `ArmMoveJ` / `MoveJ` / `moveJ` / `moveJoint`
  - 端口：
    - `jointpos`（必填）
    - `speed`（默认 0.3）
    - `acc`（默认 0.3）
    - `jerk`（默认 0.6）

- `ArmPathDownload` / `path_download`
  - 含义：路径/轨迹下发相关（用于执行轨迹前的准备）。
  - 端口：以实现为准（后续如需可以补充更细的 schema 说明）。

- 简单操作（`ArmSimpleOpAction` 封装，通常无额外端口）
  - `fault_reset` / `FaultReset`
  - `reset_system` / `ResetSystem`
  - `slow_speed` / `slowSpeed`
  - `quick_stop` / `quickStop`
  - `emergency_stop` / `EmergencyStop`

### 1.2 查询/条件（Query / Condition）

- 布尔查询（`ArmBoolQueryAction`）
  - `IsArmReady`（op: `is_arm_ready`）
  - `IsPowerOn`（op: `is_power_on`）
  - `IsStartSignal`（op: `is_start_signal`）
  - `IsStopSignal`（op: `is_stop_signal`）
  - `IsTrajectoryComplete`（op: `is_trajectory_complete`）
  - `IsAllTrajectoriesComplete`（op: `is_all_trajectories_complete`）
  - `wait_for_start` / `WaitForStart`（op: `wait_for_start`）
  - `execute_trajectory` / `ExecuteTrajectory`（op: `execute_trajectory`）

- 机器人状态查询
  - `get_robot_mode` / `GetRobotMode`
  - `get_joint_actual_pos` / `GetJointActualPos` / `ArmGetJointActualPos`

## 2) 示例 XML 在哪里

### 2.1 默认运行使用的 bt.xml

- bt_service 固定从当前工作目录读取 `./bt.xml`（相对路径）。
- 本仓库默认在 `Workstation/resources/bt.xml` 提供一份示例。
- 构建后通常会在 `Workstation/build/bt.xml` 自动生成软链接指向 `Workstation/resources/bt.xml`（便于在 `Workstation/build` 直接运行）。

建议编辑：`Workstation/resources/bt.xml`，让热加载和版本管理更清晰。

### 2.2 更多参考 XML

仓库根目录 `_demo/` 下有大量 BT XML（历史/实验用）：
- `../../_demo/bt*.xml`

这些 XML 可能使用了不同的节点命名、或依赖更完整的外部节点集。若要迁移到 Workstation，请优先按本工程 `arm_nodes.cpp` 的节点名称对齐。

## 3) 常见坑

- `bt.xml` 相对路径：systemd 部署时务必设置 `WorkingDirectory=`，保证运行目录下存在 `bt.xml`。
- MoveL/MoveJ 安全：建议先在 arm_control 开启 `WXZ_ARM_DRY_RUN=1` 做命令/参数验证，再解除 dry-run。

