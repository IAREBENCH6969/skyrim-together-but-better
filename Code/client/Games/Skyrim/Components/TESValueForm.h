#pragma once

#include <Components/BaseFormComponent.h>

// Gold value of an item (weapons, armor, books, ingredients, misc items, ...). Reach it with Cast<TESValueForm>(form).
struct TESValueForm : BaseFormComponent
{
    int32_t value;
};

static_assert(offsetof(TESValueForm, value) == 0x8);

// Weight of an item. Reach it with Cast<TESWeightForm>(form).
struct TESWeightForm : BaseFormComponent
{
    float weight;
};

static_assert(offsetof(TESWeightForm, weight) == 0x8);
