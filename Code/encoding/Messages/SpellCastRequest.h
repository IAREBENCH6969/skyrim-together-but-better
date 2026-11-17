#pragma once

#include "Message.h"
#include <Structs/GameId.h>
#include <Structs/Inventory.h>

struct SpellCastRequest final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kSpellCastRequest;

    SpellCastRequest()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const SpellCastRequest& acRhs) const noexcept { return CasterId == acRhs.CasterId && SpellFormId == acRhs.SpellFormId && CastingSource == acRhs.CastingSource && IsDualCasting == acRhs.IsDualCasting && DesiredTarget == acRhs.DesiredTarget && CustomAlchemy == acRhs.CustomAlchemy && GetOpcode() == acRhs.GetOpcode(); }

    uint32_t CasterId;
    GameId SpellFormId{};
    int32_t CastingSource;
    bool IsDualCasting;
    uint32_t DesiredTarget{};
    // Set when SpellFormId is a player-made potion or poison, which only exists on the sender.
    Inventory::AlchemyData CustomAlchemy{};
};
