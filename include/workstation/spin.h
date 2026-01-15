#pragma once

// Workstation public facade：实际实现位于 MotionCore/framework。

#include "framework/spin.h"

namespace wxz::workstation {

using wxz::framework::spin_once;
using wxz::framework::spin_some;
using wxz::framework::spin;

} // namespace wxz::workstation
