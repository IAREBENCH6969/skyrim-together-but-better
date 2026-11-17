#pragma once

#include "Message.h"

// Player revive: a downed player (health at 0, in bleedout) can be brought back by another player using a healing
// potion, until the server's revive window runs out and they respawn as usual.
enum class ReviveAction : uint8_t
{
    Downed, // the sender went down
    Up,     // the sender is up again (respawned, or revived)
    Revive  // the sender used a healing potion on PlayerId; Health is what they get back
};

struct ReviveRequest final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kReviveRequest;

    ReviveRequest()
        : ClientMessage(Opcode)
    {
    }

    virtual ~ReviveRequest() = default;

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const ReviveRequest& acRhs) const noexcept
    {
        return GetOpcode() == acRhs.GetOpcode() && Action == acRhs.Action && PlayerId == acRhs.PlayerId && Health == acRhs.Health && Seconds == acRhs.Seconds;
    }

    ReviveAction Action{};
    uint32_t PlayerId{};
    float Health{};
    // Downed: seconds until that player respawns if nobody revives them.
    float Seconds{};
};
