# Workstation 服务目录规范

本目录将 Workstation 各功能服务节点视为“独立模块”，每个服务都有清晰的 `include/` 与 `src/` 分层，便于扩展、测试与部署。

## 目标
- 为每个服务提供稳定的入口（`main.cpp`）与实现（`app.cpp/.h`），避免单文件耦合。
- 统一命名与目录结构，便于独立开发与跨服务协作。
机械臂控制服务：
- 订阅 `/arm/command`（EventDTO，CDR 编码），解析其中 payload（KV 串）并执行控制指令
- 发布 `/arm/status`（EventDTO，CDR 编码），payload 为 KV 串（状态/告警/错误码等）
```
Workstation/services/
  <service_name>/
    include/
通过发布 `/arm/command` 控制机械臂
通过订阅 `/arm/status` 获取机械臂状态
          app.h
    src/
      app.cpp
      main.cpp
```

示例：
- 行为树服务：`bt_service`
- 机械臂控制：`arm_control`
- 任务调度：`task_scheduler`
- 运动规划：`motion_planning`

## 命名约定
- 头文件路径：`wxz_workstation/<service_name>/app.h`
- 入口文件：`src/main.cpp`（仅负责解析环境/参数、初始化并运行 `App`）
- 业务实现：`src/app.cpp` + `include/.../app.h`
- 可执行产物命名与 CMake 目标遵循：`workstation_<service_name>_service`

## 代码组织与职责
- `main.cpp`
  - 解析必要环境变量（如域 ID、Topic、配置路径等）。
  - 构造 `App` 并调用 `run()`；尽量不含业务逻辑。
- `app.h/.cpp`
  - 对外暴露最小接口（通常为 `App` 类）。
  - 内部封装具体业务（DDS 通信、SDK 交互、状态管理等）。

## 头文件引用示例

```cpp
#include <wxz_workstation/bt_service/app.h>
// 或
#include <wxz_workstation/arm_control/app.h>
```

## CMake 约定（简述）
- 每个服务一个 `add_executable(workstation_<service>_service ...)`，源文件仅包含 `src/main.cpp` 与 `src/app.cpp`。
- `target_include_directories` 指向对应 `include/` 目录。
- 链接依赖：统一链接 `MotionCore` 与 `Threads::Threads`；行为树服务额外链接 BehaviorTree.CPP v3；机械臂控制按需链接/加载 CGXi SDK。
- 详见上层 [Workstation/CMakeLists.txt](../CMakeLists.txt)。

## 依赖与环境变量（摘要）
- 通用：
  - `WXZ_DOMAIN_ID`：DDS Domain ID（默认 11）。
  - Topic：各服务的 command/status（如 `/arm/command`、`/arm/status`、task 相关 topic 等）。
- 行为树（BT）：
  - `WXZ_BT_XML`：行为树 XML 路径。
  - `WXZ_BT_GROOT` / `WXZ_BT_GROOT_PORT`：是否启用 Groot1 及端口。
  - `WXZ_BT_GROOT_RETRY`：Groot 端口冲突自动重试次数（默认 5，每次端口 +1）。
- 机械臂控制（ARM）：
  - `WXZ_ARM_IP` / `WXZ_ARM_PORT` / `WXZ_ARM_PASS`：控制端连接配置。
  - 机械臂 SDK 以直链方式集成，无需设置运行期 `WXZ_ARM_SDK_SO`。

## 构建与运行（示例）

构建（启用 BT 与真实 SDK 直链示例）：

```bash
WS=$(pwd)

# 先确保 MotionCore 已安装，并提供给 Workstation 的 find_package 使用
# 例如：$WS/MotionCore/_install

cd $WS/Workstation
mkdir -p build
cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$WS/MotionCore/_install" \
  -DWXZ_WORKSTATION_ENABLE_BT=ON \
  -DWXZ_ARM_LINK_SDK=ON

make -j"$(nproc)"
```

本地运行（两个终端分别启动，推荐用 env 注入）：

```bash
$WS/Workstation/build/workstation_arm_control_service
$WS/Workstation/build/workstation_bt_service
```

端口冲突（Groot）时可切换端口：

```bash
export WXZ_BT_GROOT_PORT=2666
$WS/Workstation/build/workstation_bt_service
```


建议做法：两个终端分别启动，配置通过 env 注入（见 Workstation/docs）。

## 迁移策略与清理
- 旧的扁平 `Workstation/src/workstation_*_service.cpp` 已被拆分到各自的 `services/*` 目录。
- CLI 工具程序已移除；本地联调建议使用手动终端启动，启动脚本可按你们的 YAML 配置自建。
- 新增服务时，按本规范建立 `include/` 与 `src/` 并在上层 CMake 添加目标与依赖。

## 备注
- 行为树依赖：可通过 `-DWXZ_WORKSTATION_BT_ROOT=/path/to/BehaviorTree.CPP` 或 `-DWXZ_WORKSTATION_BT_FETCHCONTENT=ON` 获取。
- 机械臂 SDK：必须使用 `-DWXZ_ARM_LINK_SDK=ON` 进行直链构建（不再支持运行期 dlopen）。构建会设置运行期 RPATH 以便解析 SDK 依赖。
