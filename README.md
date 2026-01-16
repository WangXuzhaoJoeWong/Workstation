# Workstation

Workstation 是应用工程，依赖 MotionCore（SDK），用于运行机器人现场服务（例如 `arm_control`、`bt_service`）。

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
```
