#pragma once

#include "Message.h"

enum class ReviveEvent : uint8_t
{
    Downed, // PlayerId went down and can be revived
    Up,     // PlayerId is up again (or left)
    Revived // you were revived by PlayerId and get Health back
};

struct NotifyRevive final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyRevive;

    NotifyRevive()
        : ServerMessage(Opcode)
    {
    }

    virtual ~NotifyRevive() = default;

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyRevive& acRhs) const noexcept
    {
        return GetOpcode() == acRhs.GetOpcode() && Event == acRhs.Event && PlayerId == acRhs.PlayerId && Health == acRhs.Health && Seconds == acRhs.Seconds;
    }

    ReviveEvent Event{};
    uint32_t PlayerId{};
    float Health{};
    // Downed: seconds until that player respawns if nobody revives them.
    float Seconds{};
};
