#pragma once

#include "MagicItem.h"

#include <Structs/Inventory.h>

struct AlchemyItem : MagicItem
{
    // Builds (or reuses, the engine dedupes identical effect lists) a player-made potion or poison from its effects.
    // The result carries one engine reference that the caller owns: release it with
    // BGSCreatedObjectManager::DecrementRef, see CreatedAlchemyItemRef. Returns nullptr if an effect can't be resolved.
    static AlchemyItem* Create(const Inventory::AlchemyData& acData) noexcept;

    bool IsPoison() const noexcept;
    void GetAlchemyData(Inventory::AlchemyData& aData) const noexcept;

    uint8_t unk90[0x138 - 0x90];
    int32_t costOverride; // ENIT
    uint32_t flags;
    uint8_t unk140[0x168 - 0x140];
};

static_assert(offsetof(AlchemyItem, costOverride) == 0x138);
// The engine allocates 0x168 bytes for a created alchemy item (BGSCreatedObjectManager create callback).
static_assert(sizeof(AlchemyItem) == 0x168);

// Owns the engine reference returned by AlchemyItem::Create, like the BSTSmartPointer the game uses for the same call.
struct CreatedAlchemyItemRef
{
    explicit CreatedAlchemyItemRef(AlchemyItem* apItem) noexcept
        : pItem(apItem)
    {
    }
    ~CreatedAlchemyItemRef() noexcept;

    CreatedAlchemyItemRef(const CreatedAlchemyItemRef&) = delete;
    CreatedAlchemyItemRef& operator=(const CreatedAlchemyItemRef&) = delete;

    AlchemyItem* pItem;
};
