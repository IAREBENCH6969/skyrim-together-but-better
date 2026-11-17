#pragma once

#include "Message.h"

#include <Structs/Inventory.h>
#include <Structs/TradeTypes.h>

struct TradeRequest final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kTradeRequest;

    TradeRequest()
        : ClientMessage(Opcode)
    {
    }

    virtual ~TradeRequest() = default;

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const TradeRequest& acRhs) const noexcept
    {
        return GetOpcode() == acRhs.GetOpcode() && Action == acRhs.Action && PlayerId == acRhs.PlayerId && Ready == acRhs.Ready && Offer == acRhs.Offer;
    }

    TradeAction Action{};
    uint32_t PlayerId{};
    bool Ready{};
    Inventory Offer{};
};
