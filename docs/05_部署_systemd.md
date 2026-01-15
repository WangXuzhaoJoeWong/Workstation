# 部署（systemd / target）

本文档给出一个可落地的 systemd 部署方式，核心目标是：
- 统一用 `/etc/workstation/workstation.env` 做配置入口（env-only）
- 确保 `workstation_bt_service` 的工作目录下存在 `./bt.xml`

> 注意：本仓库不强绑定安装路径。你可以把二进制放到任意目录（如 `/opt/wxz_robot/bin`），只要 unit 文件里的路径与权限正确即可。

## 1) 文件准备

### 1.1 环境变量文件

- 参考样例：`Workstation/resources/workstation.env.sample`
- 现场建议：复制为 `/etc/workstation/workstation.env`

### 1.2 bt.xml

bt_service 固定读取当前工作目录下的 `./bt.xml`。

两种常见做法：

A) 让工作目录就是包含 bt.xml 的目录
- 例如：`/opt/wxz_robot/workstation` 下放 `bt.xml`，并设置 `WorkingDirectory=/opt/wxz_robot/workstation`

B) 在工作目录里放软链接
- 例如：`/opt/wxz_robot/workstation/bt.xml -> /etc/workstation/bt.xml`

## 2) systemd unit 示例

下面给出两个最小 unit 示例（可以按现场目录调整）。

### 2.1 arm_control

`/etc/systemd/system/workstation_arm_control.service`：

```ini
[Unit]
Description=Workstation Arm Control Service
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
EnvironmentFile=/etc/workstation/workstation.env
WorkingDirectory=/opt/wxz_robot/workstation
ExecStart=/opt/wxz_robot/bin/workstation_arm_control_service
Restart=always
RestartSec=1

[Install]
WantedBy=multi-user.target
```

### 2.2 bt_service

`/etc/systemd/system/workstation_bt.service`：

```ini
[Unit]
Description=Workstation BehaviorTree Service
After=network-online.target workstation_arm_control.service
Wants=network-online.target

[Service]
Type=simple
EnvironmentFile=/etc/workstation/workstation.env
WorkingDirectory=/opt/wxz_robot/workstation
ExecStart=/opt/wxz_robot/bin/workstation_bt_service
Restart=always
RestartSec=1

[Install]
WantedBy=multi-user.target
```

关键点：

- `WorkingDirectory=` 必须保证目录下有 `bt.xml`。
- 两个服务建议共享同一份 `EnvironmentFile=`，确保 domain/topic/schema 一致。

关于 metrics 端口：

- 两个服务都支持 HTTP `/metrics`，默认端口分别是 bt_service=9100、arm_control=9101。
- 如果使用同一份 `EnvironmentFile=` 启用 metrics，建议不要在全局 env 文件里设置 `WXZ_METRICS_HTTP_PORT`，避免端口冲突。
- 如需改端口，建议在各自 unit 里用 `Environment=WXZ_METRICS_HTTP_PORT=...` 单独覆盖。

## 3) systemd override（推荐）

如果 unit 文件是随包安装的，现场只想改 `WorkingDirectory` 或 `EnvironmentFile`，建议使用 override：

- `systemctl edit workstation_bt.service`

例如：

```ini
[Service]
EnvironmentFile=/etc/workstation/workstation.env
WorkingDirectory=/opt/wxz_robot/workstation
```

## 4) 常用命令

- `systemctl daemon-reload`
- `systemctl enable --now workstation_arm_control.service`
- `systemctl enable --now workstation_bt.service`
- `journalctl -u workstation_bt.service -f`

## 5) 现场排障提示

- `operate_timeout (code=3)`：优先检查网线/交换机、IP/端口、以及防火墙。
- MoveL/MoveJ 先 dry-run：可先 `WXZ_ARM_DRY_RUN=1`，验证参数不会“度当弧度”等明显错误，再关闭 dry-run。

