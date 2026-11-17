#include <Messages/NotifyRevive.h>
#include <TiltedCore/Serialization.hpp>

void NotifyRevive::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    aWriter.WriteBits(static_cast<uint8_t>(Event), 8);
    Serialization::WriteVarInt(aWriter, PlayerId);
    Serialization::WriteFloat(aWriter, Health);
}

void NotifyRevive::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);

    uint64_t value = 0;
    aReader.ReadBits(value, 8);
    Event = static_cast<ReviveEvent>(value);
    PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    Health = Serialization::ReadFloat(aReader);
}
