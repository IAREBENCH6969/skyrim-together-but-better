#include "BGSCreatedObjectManager.h"

BGSCreatedObjectManager* BGSCreatedObjectManager::Get() noexcept
{
    POINTER_SKYRIMSE(BGSCreatedObjectManager*, pObjManager, 400320);
    return *pObjManager.Get();
}

void BGSCreatedObjectManager::IncrementRef(AlchemyItem* apItem) noexcept
{
    TP_THIS_FUNCTION(TIncrementRef, void, BGSCreatedObjectManager, AlchemyItem*);
    POINTER_SKYRIMSE(TIncrementRef, incrementRef, 36170);
    TiltedPhoques::ThisCall(incrementRef, this, apItem);
}

void BGSCreatedObjectManager::DecrementRef(AlchemyItem* apItem) noexcept
{
    TP_THIS_FUNCTION(TDecrementRef, void, BGSCreatedObjectManager, AlchemyItem*);
    POINTER_SKYRIMSE(TDecrementRef, decrementRef, 36171);
    TiltedPhoques::ThisCall(decrementRef, this, apItem);
}
