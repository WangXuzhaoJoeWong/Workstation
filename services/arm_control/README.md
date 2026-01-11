
# 最短运行命令（两终端）

## 终端 A（arm_control）：

```bash
REPO_ROOT=$(pwd)
BUILD_DIR=$REPO_ROOT/../build_wxz_robot

cd $BUILD_DIR/Workstation && \
LD_LIBRARY_PATH=$REPO_ROOT/Workstation/depends/CGXi_Robot_SDK/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH} \
WXZ_DOMAIN_ID=0 ./workstation_arm_control_service

```

## 终端 B（bt_service）：

```bash
REPO_ROOT=$(pwd)
BUILD_DIR=$REPO_ROOT/../build_wxz_robot

cd $BUILD_DIR/Workstation && \
WXZ_DOMAIN_ID=0 WXZ_BT_XML=$REPO_ROOT/_demo/bt_arm_control_min.xml \
./workstation_bt_service

```

## Groot1（可选）

bt_service 默认启用 Groot1（ZMQ publisher）。如果你不想开：设置 `WXZ_BT_GROOT=0`。

```bash
REPO_ROOT=$(pwd)
BUILD_DIR=$REPO_ROOT/../build_wxz_robot

cd $BUILD_DIR/Workstation && \
WXZ_DOMAIN_ID=0 \
WXZ_BT_XML=$REPO_ROOT/_demo/bt_arm_control_min.xml \
WXZ_BT_GROOT_PORT=1666 \
./workstation_bt_service
```

- 默认端口：publisher=1666，server=1667（server 端口可用 `WXZ_BT_GROOT_SERVER_PORT` 指定）
- 如果端口被占用，bt_service 会自动尝试下一个端口，并在日志里打印：`Groot1 enabled on port <p> (server <p+1>)`

# 直接用下面这条（最小 MoveL/PowerOn demo）：

```bash
REPO_ROOT=$(pwd)
BUILD_DIR=$REPO_ROOT/../build_wxz_robot

cd $BUILD_DIR/Workstation && \
WXZ_DOMAIN_ID=0 WXZ_BT_XML=$REPO_ROOT/_demo/bt_arm_control_min.xml \
./workstation_bt_service
```

# 如果你要跑“带条件守卫”的版本：

```bash
REPO_ROOT=$(pwd)
BUILD_DIR=$REPO_ROOT/../build_wxz_robot

cd $BUILD_DIR/Workstation && \
WXZ_DOMAIN_ID=0 WXZ_BT_XML=$REPO_ROOT/_demo/bt_arm_control_helpers_guarded.xml \
./workstation_bt_service

```

