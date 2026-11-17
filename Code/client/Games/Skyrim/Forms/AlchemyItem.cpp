#include "AlchemyItem.h"

#include <Systems/ModSystem.h>
#include <Magic/EffectItem.h>
#include <World.h>
#include <Games/BGSCreatedObjectManager.h>

AlchemyItem* AlchemyItem::Create(const Inventory::AlchemyData& acData) noexcept
{
    // Both write the created form into a BSTSmartPointer (one pointer wide) that holds one reference.
    TP_THIS_FUNCTION(TAddCreatedAlchemyItem, void, BGSCreatedObjectManager, AlchemyItem** appResult, GameArray<EffectItem>* apEffects);
    POINTER_SKYRIMSE(TAddCreatedAlchemyItem, addPotion, 36167);
    POINTER_SKYRIMSE(TAddCreatedAlchemyItem, addPoison, 36168);

    if (acData.IsEmpty())
        return nullptr;

    ModSystem& modSystem = World::Get().GetModSystem();

    GameArray<EffectItem> effects{};
    effects.Resize(acData.Effects.size());
    for (uint32_t i = 0; i < acData.Effects.size(); i++)
    {
        const Inventory::EffectItem& cEffect = acData.Effects[i];

        EffectItem effectItem{};
        effectItem.data.fMagnitude = cEffect.Magnitude;
        effectItem.data.iArea = cEffect.Area;
        effectItem.data.iDuration = cEffect.Duration;
        effectItem.fRawCost = cEffect.RawCost;
        effectItem.pEffectSetting = Cast<EffectSetting>(TESForm::GetById(modSystem.GetGameId(cEffect.EffectId)));

        // The engine compares and names potions through the effect setting, so don't hand it a null one.
        if (!effectItem.pEffectSetting)
        {
            spdlog::error("{}: effect setting not found: {:X}:{:X}", __FUNCTION__, cEffect.EffectId.ModId, cEffect.EffectId.BaseId);
            return nullptr;
        }

        effects[i] = effectItem;
    }

    AlchemyItem* pItem = nullptr;
    TiltedPhoques::ThisCall(acData.IsPoison ? addPoison : addPotion, BGSCreatedObjectManager::Get(), &pItem, &effects);

    return pItem;
}

bool AlchemyItem::IsPoison() const noexcept
{
    // Virtual at vtable + 0x308; BGSCreatedObjectManager uses it to choose between its potion and poison tables.
    using TIsPoison = bool(const AlchemyItem*);
    TIsPoison* pIsPoison = (*reinterpret_cast<TIsPoison* const* const*>(this))[0x308 / sizeof(void*)];
    return pIsPoison(this);
}

void AlchemyItem::GetAlchemyData(Inventory::AlchemyData& aData) const noexcept
{
    ModSystem& modSystem = World::Get().GetModSystem();

    aData.IsPoison = IsPoison();
    aData.Effects.clear();

    for (EffectItem* pEffectItem : listOfEffects)
    {
        if (!pEffectItem || !pEffectItem->pEffectSetting)
            continue;

        Inventory::EffectItem effect;
        effect.Magnitude = pEffectItem->data.fMagnitude;
        effect.Area = pEffectItem->data.iArea;
        effect.Duration = pEffectItem->data.iDuration;
        effect.RawCost = pEffectItem->fRawCost;
        modSystem.GetServerModId(pEffectItem->pEffectSetting->formID, effect.EffectId);
        aData.Effects.push_back(effect);
    }
}

CreatedAlchemyItemRef::~CreatedAlchemyItemRef() noexcept
{
    if (pItem)
        BGSCreatedObjectManager::Get()->DecrementRef(pItem);
}
