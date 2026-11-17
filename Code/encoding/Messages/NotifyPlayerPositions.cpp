#include <Messages/NotifyPlayerPositions.h>
#include <TiltedCore/Serialization.hpp>

void NotifyPlayerPositions::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, Players.size());
    for (const auto& entry : Players)
    {
        Serialization::WriteVarInt(aWriter, entry.PlayerId);
        entry.WorldSpaceId.Serialize(aWriter);
        entry.Position.Serialize(aWriter);
        Serialization::WriteFloat(aWriter, entry.Heading);
        Serialization::WriteBool(aWriter, entry.Inside);
        Serialization::WriteVarInt(aWriter, entry.ColorIndex);
    }
}

void NotifyPlayerPositions::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);

    const auto cCount = Serialization::ReadVarInt(aReader);
    Players.clear();
    for (uint64_t i = 0; i < cCount && i < 1024; ++i)
    {
        Entry entry{};
        entry.PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
        entry.WorldSpaceId.Deserialize(aReader);
        entry.Position.Deserialize(aReader);
        entry.Heading = Serialization::ReadFloat(aReader);
        entry.Inside = Serialization::ReadBool(aReader);
        entry.ColorIndex = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
        Players.push_back(entry);
    }
}
