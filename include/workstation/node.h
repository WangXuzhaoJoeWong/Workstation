#pragma once

// Workstation public facade：实际实现位于 MotionCore/framework。

#include "framework/node.h"

namespace wxz::workstation {

// 兼容别名：实际实现位于 MotionCore 的 framework 层。
using SubscriptionStats = wxz::framework::SubscriptionStats;
using EventDtoSubscription = wxz::framework::EventDtoSubscription;
using EventDtoPublisher = wxz::framework::EventDtoPublisher;
using TextSubscription = wxz::framework::TextSubscription;
using Node = wxz::framework::Node;

} // namespace wxz::workstation
