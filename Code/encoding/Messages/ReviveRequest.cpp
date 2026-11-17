#include <Messages/ReviveRequest.h>
#include <TiltedCore/Serialization.hpp>

void ReviveRequest::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    aWriter.WriteBits(static_cast<uint8_t>(Action), 8);
    Serialization::WriteVarInt(aWriter, PlayerId);
    Serialization::WriteFloat(aWriter, Health);
    Serialization::WriteFloat(aWriter, Seconds);
}

void ReviveRequest::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);

    uint64_t action = 0;
    aReader.ReadBits(action, 8);
    Action = static_cast<ReviveAction>(action);
    PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    Health = Serialization::ReadFloat(aReader);
    Seconds = Serialization::ReadFloat(aReader);
}
