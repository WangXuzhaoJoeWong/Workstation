# Workstation

Workstation 是应用工程，依赖 MotionCore（SDK），用于运行机器人现场服务（例如 `arm_control`、`bt_service`）。

- Workstation 不负责构建/安装 MotionCore；请先安装 MotionCore，再构建 Workstation
- 在 wxz_robot 工作空间内，开发态推荐让 MotionCore 安装到其 build 目录，然后 Workstation 通过 `CMAKE_PREFIX_PATH` 指向该目录

## 文档入口

- 文档索引：Workstation/docs/README.md
- 新对话快速衔接：Workstation/docs/00_架构与开发目的.md
- 编译/运行：Workstation/docs/01_快速开始.md

## 构建（最小示例）

```bash
WS=$(pwd)

# 先构建并安装 MotionCore 到其 build 目录
cd MotionCore && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j"$(nproc)"
./install.sh

# 再构建 Workstation
cd "$WS/Workstation" && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$WS/MotionCore/build"
cmake --build . -j"$(nproc)"

# （可选）安装到 Workstation/build
./install.sh
```

## 运行与部署

运行、systemd、常见故障与排障入口见：Workstation/docs/README.md
