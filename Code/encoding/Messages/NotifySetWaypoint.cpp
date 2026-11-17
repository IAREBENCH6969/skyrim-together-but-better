#include <Messages/NotifySetWaypoint.h>
#include <TiltedCore/Serialization.hpp>

void NotifySetWaypoint::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, PlayerId);
    Position.Serialize(aWriter);
    WorldSpaceFormID.Serialize(aWriter);
}

void NotifySetWaypoint::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);

    PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    Position.Deserialize(aReader);
    WorldSpaceFormID.Deserialize(aReader);
}
