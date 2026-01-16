#include "arm_status_cache.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "dto/event_dto.h"
#include "strand.h"
#include "arm_types.h"
#include "service_common.h"

namespace wxz::workstation::bt_service {

std::unique_ptr<wxz::workstation::EventDtoSubscription> install_arm_status_cache_updater(
    wxz::workstation::Node& node,
    const std::string& status_dto_topic,
    const std::string& status_dto_schema,
    wxz::core::Strand& ingress_strand,
    std::size_t dto_max_payload,
    std::size_t pool_buffers,
    ArmRespCache& arm_cache) {
    wxz::workstation::EventDtoSubscription::Options opts;
    opts.qos = wxz::core::default_reliable_qos();
    opts.dto_max_payload = dto_max_payload;
    opts.pool_buffers = pool_buffers;

    return node.create_subscription_eventdto_on(
        ingress_strand,
        status_dto_topic,
        status_dto_schema,
        [&](const ::EventDTO& dto) {
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
        },
        std::move(opts));
}

}  // namespace wxz::workstation::bt_service
