#include <Messages/NotifyTrade.h>
#include <TiltedCore/Serialization.hpp>

void NotifyTrade::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    aWriter.WriteBits(static_cast<uint8_t>(Event), 8);
    Serialization::WriteVarInt(aWriter, PartnerId);
    aWriter.WriteBits(static_cast<uint8_t>(Reason), 8);
    Serialization::WriteBool(aWriter, SelfReady);
    Serialization::WriteBool(aWriter, PartnerReady);
    Items.Serialize(aWriter);
}

void NotifyTrade::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);

    uint64_t value = 0;
    aReader.ReadBits(value, 8);
    Event = static_cast<TradeEvent>(value);
    PartnerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    value = 0;
    aReader.ReadBits(value, 8);
    Reason = static_cast<TradeCloseReason>(value);
    SelfReady = Serialization::ReadBool(aReader);
    PartnerReady = Serialization::ReadBool(aReader);
    Items.Deserialize(aReader);
}
