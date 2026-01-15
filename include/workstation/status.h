#pragma once

// Workstation public facade：实际实现位于 MotionCore/framework。

#include "framework/status.h"

namespace wxz::workstation {

// 兼容别名：实际实现位于 MotionCore 的 framework 层。
using Status = wxz::framework::Status;

} // namespace wxz::workstation
