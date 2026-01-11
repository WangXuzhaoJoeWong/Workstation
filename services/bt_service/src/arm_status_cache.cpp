#include "wxz_workstation/bt_service/arm_status_cache.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "dto/event_dto.h"
#include "dto/event_dto_cdr.h"
#include "fastdds_channel.h"
#include "wxz_workstation/bt_service/arm_types.h"

namespace wxz::workstation::bt_service {

void install_arm_status_cache_updater(wxz::core::FastddsChannel& arm_status_dto_sub, ArmRespCache& arm_cache) {
    arm_status_dto_sub.subscribe([&](const std::uint8_t* data, std::size_t size) {
        std::vector<std::uint8_t> buf(data, data + size);
        ::EventDTO dto;
        if (!wxz::dto::decode_event_dto_cdr(buf, dto)) return;
        auto kv = EventDTOUtil::parsePayloadKv(dto.payload);

        if (!kv.count("id")) kv["id"] = dto.event_id;
        const auto it_id = kv.find("id");
        if (it_id == kv.end()) return;

        ArmResp r;
        r.ok = kv.count("ok") ? kv["ok"] : "0";
        r.code = kv.count("code") ? kv["code"] : "";
        r.err_code = kv.count("err_code") ? kv["err_code"] : "";
        r.err = kv.count("err") ? kv["err"] : "";
        r.sdk_code = kv.count("sdk_code") ? kv["sdk_code"] : "";
        r.ts_ms = now_monotonic_ms();
        r.kv = std::move(kv);

        arm_cache.put(it_id->second, std::move(r));
    });
}

}  // namespace wxz::workstation::bt_service
