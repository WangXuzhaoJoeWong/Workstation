#pragma once

namespace wxz::workstation::bt_service {

/// bt_service 进程入口。
///
/// 负责初始化运行环境，并进入主循环（由内部统一驱动 Executor/Strand）。
/// 返回值语义遵循常规 main()：0 表示正常退出，非 0 表示错误退出。
int run();
}
