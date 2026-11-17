#pragma once

#include "Message.h"

#include <Structs/GameId.h>
#include <Structs/Vector3_NetQuantize.h>

/**
 * @brief Where every player is, for drawing them on the world map. Sent to everyone about once a second.
 *
 * A player who is inside an interior is reported at the last place they were outside, with Inside set (and with no
 * worldspace if they haven't been outside yet).
 * Every player also gets a colour slot, unique among the connected players and kept until they leave.
 */
struct NotifyPlayerPositions final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyPlayerPositions;

    NotifyPlayerPositions()
        : ServerMessage(Opcode)
    {
    }

    struct Entry
    {
        uint32_t PlayerId{};
        GameId WorldSpaceId{};
        Vector3_NetQuantize Position{};
        float Heading{}; // radians, clockwise from north
        bool Inside{};
        uint32_t ColorIndex{}; // the player's colour slot

        bool operator==(const Entry& acRhs) const noexcept
        {
            return PlayerId == acRhs.PlayerId && WorldSpaceId == acRhs.WorldSpaceId && Position == acRhs.Position && Heading == acRhs.Heading && Inside == acRhs.Inside &&
                   ColorIndex == acRhs.ColorIndex;
        }
    };

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyPlayerPositions& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Players == acRhs.Players; }

    TiltedPhoques::Vector<Entry> Players;
};
