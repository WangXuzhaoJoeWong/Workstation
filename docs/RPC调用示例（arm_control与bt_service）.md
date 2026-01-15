# RPC 调用示例（arm_control 与 bt_service）

本文给出“可复制粘贴”的最小 RPC 调用方式，方便联调。

约定：
- RPC 基于 MotionCore `wxz::core::rpc::RpcClient/RpcServer`。
- topic 命名已收敛为：`/svc/<service>/rpc/request` 与 `/svc/<service>/rpc/reply`。
- 负载是 JSON（UTF-8 文本），由底层 FastDDS topic 传输。

## 0. 服务端启用（环境变量）

### bt_service

```bash
export WXZ_BT_RPC_ENABLE=1
export WXZ_BT_RPC_REQUEST_TOPIC=/svc/bt_service/rpc/request
export WXZ_BT_RPC_REPLY_TOPIC=/svc/bt_service/rpc/reply
export WXZ_BT_RPC_SERVICE_NAME=workstation_bt_service
```

### arm_control

```bash
export WXZ_ARM_RPC_ENABLE=1
export WXZ_ARM_RPC_REQUEST_TOPIC=/svc/arm_control/rpc/request
export WXZ_ARM_RPC_REPLY_TOPIC=/svc/arm_control/rpc/reply
export WXZ_ARM_RPC_SERVICE_NAME=workstation_arm_control_service
```

> `WXZ_DOMAIN_ID` 需要两端一致（同一个 DDS domain）。

## 1. bt_service：bt.ping / bt.reload / bt.stop

### 请求 JSON（示例）

`bt.ping`：

```json
{"op":"bt.ping","params":{}}
```

`bt.reload`：

```json
{"op":"bt.reload","params":{}}
```

`bt.stop`：

```json
{"op":"bt.stop","params":{}}
```

> 说明：当前 bt_service 的 RPC handler 不依赖复杂入参（最小可用控制面）。

## 2. arm_control：arm.ping / arm.command

### 请求 JSON（示例）

`arm.ping`：

```json
{"op":"arm.ping","params":{}}
```

`arm.command`（KV raw 透传风格，保持与现有 domain 兼容）：

```json
{
  "op": "arm.command",
  "params": {
    "op": "set_digital_out",
    "channel": 1,
    "value": 1
  }
}
```

> 说明：当前 arm_control 的 `arm.command` handler 要求 `params.op` 存在；其它字段会被转换成 `k=v` 形式并交给 domain/SDK 处理。

## 3. 用一个最小客户端发起调用（C++）

如果你希望用 MotionCore 自带的最小 client 代码联调（推荐，避免引入额外工具），可在任意小程序里这样写：

```cpp
#include <chrono>
#include <iostream>
#include <thread>

#include "executor.h"
#include "rpc/rpc_client.h"

int main() {
  using namespace std::chrono_literals;

  wxz::core::Executor exec({.threads = 0});
  (void)exec.start();
  std::thread spin([&] { exec.spin(); });

  wxz::core::rpc::RpcClient client({
    .domain = 0,
    .request_topic = "/svc/arm_control/rpc/request",
    .reply_topic = "/svc/arm_control/rpc/reply",
    .client_id_prefix = "cli",
  });
  client.bind_scheduler(exec);
  (void)client.start();

  auto res = client.call("arm.ping",
                        wxz::core::rpc::RpcClient::Json::object(),
                        200ms);

  std::cout << "ok=" << res.ok() << " code=" << res.code << " reason='" << res.reason << "'\n";

  client.stop();
  exec.stop();
  spin.join();
  return 0;
}
```

## 4. 运行期排障（常见）

- 两端 `WXZ_DOMAIN_ID` 不一致：互相收不到。
- topic 写错：请以 `/svc/<service>/rpc/{request,reply}` 为准，或检查 env 覆盖。
- 若启用了 metrics sink：可观察 `wxz.rpc.*` 与 `wxz.executor.*` 指标，判断是未收到、被 drop、还是超时。
