#pragma once

namespace wxz::workstation::arm_control {
/// arm_control 进程入口。
///
/// 负责初始化 arm_control 运行环境，并进入主循环。
/// 返回值语义遵循常规 main()：0 表示正常退出，非 0 表示错误退出。
int run();
}
