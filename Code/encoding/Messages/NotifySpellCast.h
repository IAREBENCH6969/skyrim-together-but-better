#pragma once

#include "Message.h"
#include <Structs/GameId.h>
#include <Structs/Inventory.h>

struct NotifySpellCast final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifySpellCast;

    NotifySpellCast()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifySpellCast& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && CasterId == acRhs.CasterId && SpellFormId == acRhs.SpellFormId && CastingSource == acRhs.CastingSource && IsDualCasting == acRhs.IsDualCasting && DesiredTarget == acRhs.DesiredTarget && CustomAlchemy == acRhs.CustomAlchemy; }

    uint32_t CasterId;
    GameId SpellFormId{};
    int32_t CastingSource;
    bool IsDualCasting;
    uint32_t DesiredTarget;
    // Set when SpellFormId is a player-made potion or poison, which only exists on the sender.
    Inventory::AlchemyData CustomAlchemy{};
};
