#pragma once

#include <cstddef>
#include <cstdint>

namespace wxz::core {
class FastddsChannel;
}

namespace wxz::workstation::bt_service {

class ArmRespCache;

void install_arm_status_cache_updater(wxz::core::FastddsChannel& arm_status_dto_sub, ArmRespCache& arm_cache);

}  // namespace wxz::workstation::bt_service
