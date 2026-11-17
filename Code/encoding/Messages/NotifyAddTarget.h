#pragma once

#include "Message.h"
#include <Structs/GameId.h>
#include <Structs/Inventory.h>

struct NotifyAddTarget final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyAddTarget;

    NotifyAddTarget()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyAddTarget& acRhs) const noexcept
    {
        return GetOpcode() == acRhs.GetOpcode() && TargetId == acRhs.TargetId && CasterId == acRhs.CasterId && SpellId == acRhs.SpellId &&
               EffectId == acRhs.EffectId && Magnitude == acRhs.Magnitude && IsDualCasting == acRhs.IsDualCasting &&
               ApplyHealPerkBonus == acRhs.ApplyHealPerkBonus && ApplyStaminaPerkBonus == acRhs.ApplyStaminaPerkBonus && CustomAlchemy == acRhs.CustomAlchemy;
    }

    uint32_t TargetId{};
    uint32_t CasterId{};
    GameId SpellId{};
    GameId EffectId{};
    float Magnitude{};
    bool IsDualCasting{};
    bool ApplyHealPerkBonus{};
    bool ApplyStaminaPerkBonus{};
    // Set when SpellId is a player-made potion or poison, which only exists on the sender (see Inventory::AlchemyData).
    Inventory::AlchemyData CustomAlchemy{};
};
