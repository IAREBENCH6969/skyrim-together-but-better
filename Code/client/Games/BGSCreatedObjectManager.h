#pragma once

struct AlchemyItem;

struct BGSCreatedObjectManager
{
    static BGSCreatedObjectManager* Get() noexcept;

    // Player-made potions and poisons are reference counted by the manager and destroyed when the count reaches 0.
    // The game adds one reference whenever it puts a created potion in an inventory.
    void IncrementRef(AlchemyItem* apItem) noexcept;
    void DecrementRef(AlchemyItem* apItem) noexcept;
};
