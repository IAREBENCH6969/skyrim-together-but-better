#pragma once

#include "Message.h"

#include <Structs/Inventory.h>
#include <Structs/TradeTypes.h>

struct NotifyTrade final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyTrade;

    NotifyTrade()
        : ServerMessage(Opcode)
    {
    }

    virtual ~NotifyTrade() = default;

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyTrade& acRhs) const noexcept
    {
        return GetOpcode() == acRhs.GetOpcode() && Event == acRhs.Event && PartnerId == acRhs.PartnerId && Reason == acRhs.Reason && SelfReady == acRhs.SelfReady &&
               PartnerReady == acRhs.PartnerReady && Items == acRhs.Items;
    }

    TradeEvent Event{};
    uint32_t PartnerId{};
    TradeCloseReason Reason{};
    bool SelfReady{};
    bool PartnerReady{};
    Inventory Items{};
};
