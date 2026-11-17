#include <Messages/TradeRequest.h>
#include <TiltedCore/Serialization.hpp>

void TradeRequest::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    aWriter.WriteBits(static_cast<uint8_t>(Action), 8);
    Serialization::WriteVarInt(aWriter, PlayerId);
    Serialization::WriteBool(aWriter, Ready);
    Offer.Serialize(aWriter);
}

void TradeRequest::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);

    uint64_t action = 0;
    aReader.ReadBits(action, 8);
    Action = static_cast<TradeAction>(action);
    PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    Ready = Serialization::ReadBool(aReader);
    Offer.Deserialize(aReader);
}
