#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Allocator.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "StringCache.h"
#include "Messages/StringCacheUpdate.h"

#include <catch2/catch.hpp>

#include <Messages/ClientMessageFactory.h>
#include <Messages/ServerMessageFactory.h>
#include <Structs/Vector2_NetQuantize.h>

#include <TiltedCore/Math.hpp>
#include <TiltedCore/Platform.hpp>

using namespace TiltedPhoques;

TEST_CASE("Encoding factory", "[encoding.factory]")
{
    Buffer buff(1000);

    {
        AuthenticationRequest request;
        request.Token = "TesSt";

        Buffer::Writer writer(&buff);
        request.Serialize(writer);

        Buffer::Reader reader(&buff);

        const ClientMessageFactory factory;
        auto pMessage = factory.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == request.GetOpcode());

        auto pRequest = CastUnique<AuthenticationRequest>(std::move(pMessage));
        REQUIRE(pRequest->Token == request.Token);
    }

    {
        PartyAcceptInviteRequest request;
        request.InviterId = 123456;

        Buffer::Writer writer(&buff);
        request.Serialize(writer);

        Buffer::Reader reader(&buff);

        const ClientMessageFactory factory;
        auto pMessage = factory.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == request.GetOpcode());

        auto pRequest = CastUnique<PartyAcceptInviteRequest>(std::move(pMessage));
        REQUIRE(pRequest->InviterId == request.InviterId);
    }
}

TEST_CASE("Static structures", "[encoding.static]")
{
    GIVEN("GameId")
    {
        GameId sendObjects, recvObjects;
        sendObjects.ModId = 1456987;
        sendObjects.BaseId = 0x789654;

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendObjects.Serialize(writer);

            Buffer::Reader reader(&buff);
            recvObjects.Deserialize(reader);

            REQUIRE(sendObjects == recvObjects);
        }
    }

    GIVEN("Vector3_NetQuantize")
    {
        Vector3_NetQuantize sendObjects, recvObjects;
        sendObjects.x = 142.56f;
        sendObjects.y = 45687.7f;
        sendObjects.z = -142.56f;

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendObjects.Serialize(writer);

            Buffer::Reader reader(&buff);
            recvObjects.Deserialize(reader);

            REQUIRE(sendObjects == recvObjects);
        }
    }

    GIVEN("Vector2_NetQuantize")
    {
        Vector2_NetQuantize sendObjects, recvObjects;
        sendObjects.x = 1000.89f;
        sendObjects.y = -485632.75f;

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendObjects.Serialize(writer);

            Buffer::Reader reader(&buff);
            recvObjects.Deserialize(reader);

            REQUIRE(sendObjects == recvObjects);
        }
    }

    GIVEN("Rotator2_NetQuantize")
    {
        Rotator2_NetQuantize sendObjects, recvObjects;
        sendObjects.x = 1.89f;
        sendObjects.y = TiltedPhoques::Pi * 2.0f;

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendObjects.Serialize(writer);

            Buffer::Reader reader(&buff);
            recvObjects.Deserialize(reader);

            REQUIRE(sendObjects == recvObjects);
        }
    }

    GIVEN("Rotator2_NetQuantize needing wrap")
    {
        // This test is a bit dangerous as floating errors can lead to sendObjects != recvObjects but the difference is minuscule so we don't care abut such cases
        Rotator2_NetQuantize sendObjects, recvObjects;
        sendObjects.x = -1.87f;
        sendObjects.y = static_cast<float>(TiltedPhoques::Pi) * 18.0f + 3.6f;

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendObjects.Serialize(writer);

            Buffer::Reader reader(&buff);
            recvObjects.Deserialize(reader);

            REQUIRE(sendObjects == recvObjects);
        }
    }
}

TEST_CASE("Differential structures", "[encoding.differential]")
{
    GIVEN("Full ActionEvent")
    {
        ActionEvent sendAction, recvAction;

        sendAction.ActionId = 42;
        sendAction.State1 = 6547;
        sendAction.Tick = 48;
        sendAction.ActorId = 12345678;
        sendAction.EventName = "test";
        sendAction.IdleId = 87964;
        sendAction.State2 = 8963;
        sendAction.TargetEventName = "toast";
        sendAction.TargetId = 963741;
        sendAction.Type = 4;

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendAction.GenerateDifferential(recvAction, writer);

            Buffer::Reader reader(&buff);
            recvAction.ApplyDifferential(reader);

            REQUIRE(sendAction == recvAction);
        }

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendAction.EventName = "Plot twist !";

            sendAction.GenerateDifferential(recvAction, writer);

            Buffer::Reader reader(&buff);
            recvAction.ApplyDifferential(reader);

            REQUIRE(sendAction == recvAction);
        }
    }

    GIVEN("A single cached event name")
    {
        ActionEvent sendAction, recvAction;

        TP_UNUSED(StringCache::Get().Add("test"))

        sendAction.ActionId = 42;
        sendAction.State1 = 6547;
        sendAction.Tick = 48;
        sendAction.ActorId = 12345678;
        sendAction.EventName = "test";
        sendAction.IdleId = 87964;
        sendAction.State2 = 8963;
        sendAction.TargetEventName = "toast";
        sendAction.TargetId = 963741;
        sendAction.Type = 4;

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendAction.GenerateDifferential(recvAction, writer);

            Buffer::Reader reader(&buff);
            recvAction.ApplyDifferential(reader);

            REQUIRE(sendAction == recvAction);
        }

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendAction.EventName = "Plot twist !";

            sendAction.GenerateDifferential(recvAction, writer);

            Buffer::Reader reader(&buff);
            recvAction.ApplyDifferential(reader);

            REQUIRE(sendAction == recvAction);
        }
    }

    GIVEN("Full Mods")
    {
        Mods sendMods, recvMods;

        Buffer buff(1000);
        Buffer::Writer writer(&buff);

        sendMods.ModList.push_back({"Hello", 42});
        sendMods.ModList.push_back({"Hi", 14});
        sendMods.ModList.push_back({"Test", 8});
        sendMods.ModList.push_back({"Toast", 49});

        sendMods.Serialize(writer);

        Buffer::Reader reader(&buff);
        recvMods.Deserialize(reader);

        REQUIRE(sendMods == recvMods);
    }

    GIVEN("AnimationVariables")
    {
        AnimationVariables vars, recvVars;
 
        vars.Booleans.resize(76);
        String testString("\xDE\xAD\xBE\xEF"
                          "\xDE\xAD\xBE\xEF\x76\xB");
        vars.String_to_VectorBool(testString, vars.Booleans);

        vars.Floats.push_back(1.f);
        vars.Floats.push_back(7.f);
        vars.Floats.push_back(12.f);
        vars.Floats.push_back(0.f);
        vars.Floats.push_back(145.f);
        vars.Floats.push_back(100.f);
        vars.Floats.push_back(-1.f);

        vars.Integers.push_back(0);
        vars.Integers.push_back(12000);
        vars.Integers.push_back(06);
        vars.Integers.push_back(7778);
        vars.Integers.push_back(41104539);

        Buffer buff(1000);
        {
            Buffer::Writer writer(&buff);

            vars.GenerateDiff(recvVars, writer);

            Buffer::Reader reader(&buff);
            recvVars.ApplyDiff(reader);

            REQUIRE(vars.Booleans == recvVars.Booleans);
            REQUIRE(vars.Floats == recvVars.Floats);
            REQUIRE(vars.Integers == recvVars.Integers);
        }

        vars.Booleans.resize(33);
        vars.Booleans[16] = false;
        vars.Booleans[17] = false;
        vars.Booleans[18] = false;
        vars.Booleans[19] = false;
        vars.Floats[3] = 42.f;
        vars.Integers[0] = 18;
        vars.Integers[3] = 0;

        {
            Buffer::Writer writer(&buff);

            vars.GenerateDiff(recvVars, writer);

            Buffer::Reader reader(&buff);
            recvVars.ApplyDiff(reader);

            REQUIRE(vars.Booleans == recvVars.Booleans);
            REQUIRE(vars.Floats == recvVars.Floats);
            REQUIRE(vars.Integers == recvVars.Integers);
        }
    }
}

TEST_CASE("Packets", "[encoding.packets]")
{
    SECTION("AuthenticationRequest")
    {
        Buffer buff(1000);

        AuthenticationRequest sendMessage, recvMessage;
        sendMessage.Token = "TesSt";
        sendMessage.UserMods.ModList.push_back({"Hello", 42});
        sendMessage.UserMods.ModList.push_back({"Hi", 14});
        sendMessage.UserMods.ModList.push_back({"Test", 8});
        sendMessage.UserMods.ModList.push_back({"Toast", 49});

        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    SECTION("AuthenticationResponse")
    {
        Buffer buff(1000);

        AuthenticationResponse sendMessage, recvMessage;
        sendMessage.Type = AuthenticationResponse::ResponseType::kAccepted;
        sendMessage.UserMods.ModList.push_back({"Hello", 42});
        sendMessage.UserMods.ModList.push_back({"Hi", 14});
        sendMessage.UserMods.ModList.push_back({"Test", 8});
        sendMessage.UserMods.ModList.push_back({"Toast", 49});

        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    SECTION("AssignCharacterRequest")
    {
        Buffer buff(1000);

        ActionEvent sendAction;
        sendAction.ActionId = 42;
        sendAction.State1 = 6547;
        sendAction.Tick = 48;
        sendAction.ActorId = 12345678;
        sendAction.EventName = "test";
        sendAction.IdleId = 87964;
        sendAction.State2 = 8963;
        sendAction.TargetEventName = "toast";
        sendAction.TargetId = 963741;
        sendAction.Type = 4;

        AssignCharacterRequest sendMessage, recvMessage;
        sendMessage.Cookie = 14523698;
        sendMessage.AppearanceBuffer = "toto";
        sendMessage.CellId.BaseId = 45;
        sendMessage.FormId.ModId = 48;
        sendMessage.ReferenceId.BaseId = 456799;
        sendMessage.ReferenceId.ModId = 4079;
        sendMessage.LatestAction = sendAction;
        sendMessage.Position.x = -452.4f;
        sendMessage.Position.y = 452.4f;
        sendMessage.Position.z = 125452.4f;
        sendMessage.Rotation.x = -1.87f;
        sendMessage.Rotation.y = 45.35f;

        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("ClientReferencesMoveRequest")
    {
        ClientReferencesMoveRequest sendMessage, recvMessage;
        sendMessage.Tick = 1712345678901;
        auto& move = sendMessage.Updates[1];
        move.Position.x = 1234.5f;
        move.Direction = 0.75f;
        move.HasLookAt = true;
        move.LookOffset.x = -320.f;
        move.LookOffset.y = 215.f;
        move.LookOffset.z = -48.f;

        AnimationVariables vars;
        vars.Booleans.resize(76);
        String testString("\xDE\xAD\xBE\xEF\x76\xB");
        vars.String_to_VectorBool(testString, vars.Booleans);

        vars.Floats.push_back(1.f);
        vars.Floats.push_back(7.f);
        vars.Floats.push_back(12.f);
        vars.Floats.push_back(0.f);
        vars.Floats.push_back(145.f);
        vars.Floats.push_back(100.f);
        vars.Floats.push_back(-1.f);

        vars.Integers.push_back(0);
        vars.Integers.push_back(12000);
        vars.Integers.push_back(06);
        vars.Integers.push_back(7778);
        vars.Integers.push_back(41104539);

        move.Variables = vars;

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(recvMessage.Tick == sendMessage.Tick);
        REQUIRE(recvMessage.Updates[1] == sendMessage.Updates[1]);
    }

    GIVEN("RequestActionEvents")
    {
        ActionEvent firstAction;
        firstAction.Tick = 1712345678901;
        firstAction.ActionId = 42;
        firstAction.TargetId = 963741;
        firstAction.State1 = 6547;
        firstAction.EventName = "jumpStandingStart";

        ActionEvent secondAction = firstAction;
        secondAction.Tick += 120;
        secondAction.ActionId = 43;
        secondAction.EventName = "attackStart";
        secondAction.TargetEventName = "toast";

        RequestActionEvents sendMessage, recvMessage;
        sendMessage.Actions[1] = {firstAction, secondAction};
        sendMessage.Actions[7] = {secondAction};

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("NotifyActionEvents")
    {
        ActionEvent action;
        action.Tick = 1712345678901;
        action.ActionId = 42;
        action.EventName = "blockStart";

        NotifyActionEvents sendMessage;
        sendMessage.Actions[3] = {action, action};

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        const ServerMessageFactory factory;
        auto pMessage = factory.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == sendMessage.GetOpcode());
        REQUIRE(*CastUnique<NotifyActionEvents>(std::move(pMessage)) == sendMessage);
    }

    GIVEN("RequestInventorySnapshot")
    {
        Inventory::Entry armor;
        armor.BaseId.BaseId = 0x12E46;
        armor.Count = 1;
        armor.ExtraWorn = true;

        Inventory::Entry gold;
        gold.BaseId.BaseId = 0xF;
        gold.Count = 250;

        RequestInventorySnapshot sendMessage;
        sendMessage.ServerId = 0x10042;
        sendMessage.OwnershipEpoch = 7;
        sendMessage.CurrentInventory.Entries = {armor, gold};
        sendMessage.CurrentInventory.CurrentMagicEquipment.RightHandSpell.BaseId = 0x12FCD;

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        const ClientMessageFactory factory;
        auto pMessage = factory.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == sendMessage.GetOpcode());
        REQUIRE(*CastUnique<RequestInventorySnapshot>(std::move(pMessage)) == sendMessage);
    }

    GIVEN("NotifyInventorySnapshot")
    {
        Inventory::Entry armor;
        armor.BaseId.BaseId = 0x12E46;
        armor.Count = 1;
        armor.ExtraWornLeft = true;

        NotifyInventorySnapshot sendMessage;
        sendMessage.ServerId = 0x10042;
        sendMessage.OwnershipEpoch = 7;
        sendMessage.CurrentInventory.Entries = {armor};
        sendMessage.CurrentInventory.CurrentMagicEquipment.Shout.BaseId = 0x13E07;

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        const ServerMessageFactory factory;
        auto pMessage = factory.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == sendMessage.GetOpcode());
        REQUIRE(*CastUnique<NotifyInventorySnapshot>(std::move(pMessage)) == sendMessage);
    }

    GIVEN("RequestInventorySnapshot with a player-made potion")
    {
        Inventory::EffectItem restoreHealth;
        restoreHealth.Magnitude = 37.5f;
        restoreHealth.Duration = 0;
        restoreHealth.RawCost = 0.5f;
        restoreHealth.EffectId.BaseId = 0x3EB15;

        Inventory::EffectItem fortifyHealth;
        fortifyHealth.Magnitude = 21.f;
        fortifyHealth.Duration = 60;
        fortifyHealth.RawCost = 0.35f;
        fortifyHealth.EffectId.BaseId = 0x3EAF3;

        Inventory::Entry potion;
        potion.BaseId.ModId = 0xFFFFFFFF;
        potion.Count = 3;
        potion.CustomAlchemy.Effects = {restoreHealth, fortifyHealth};

        RequestInventorySnapshot sendMessage;
        sendMessage.ServerId = 0x10042;
        sendMessage.OwnershipEpoch = 7;
        sendMessage.CurrentInventory.Entries = {potion};

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        const ClientMessageFactory factory;
        auto pMessage = factory.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == sendMessage.GetOpcode());
        auto pReceived = CastUnique<RequestInventorySnapshot>(std::move(pMessage));
        REQUIRE(*pReceived == sendMessage);
        REQUIRE(pReceived->CurrentInventory.Entries[0].CustomAlchemy.Effects.size() == 2);
    }
}

TEST_CASE("Player-made poison on a weapon", "[encoding.inventory]")
{
    Inventory::EffectItem damageHealth;
    damageHealth.Magnitude = 10.f;
    damageHealth.EffectId.BaseId = 0x3EB42;

    Inventory::Entry dagger;
    dagger.BaseId.BaseId = 0x1397E;
    dagger.Count = 1;
    dagger.ExtraPoisonId.ModId = 0xFFFFFFFF;
    dagger.ExtraPoisonCount = 1;
    dagger.ExtraPoisonData.IsPoison = true;
    dagger.ExtraPoisonData.Effects = {damageHealth};

    GIVEN("A round trip")
    {
        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        dagger.Serialize(writer);

        Buffer::Reader reader(&buff);
        Inventory::Entry received;
        received.Deserialize(reader);

        REQUIRE(received == dagger);
        REQUIRE(received.ContainsExtraData());
        REQUIRE(received.ExtraPoisonData.Effects.size() == 1);
    }

    GIVEN("A plain dagger of the same kind")
    {
        Inventory::Entry plain;
        plain.BaseId.BaseId = 0x1397E;
        plain.Count = 1;

        REQUIRE_FALSE(dagger.CanBeMerged(plain));
    }
}

TEST_CASE("Player-made potion magic effects", "[encoding.inventory]")
{
    Inventory::EffectItem damageHealth;
    damageHealth.Magnitude = 10.f;
    damageHealth.EffectId.BaseId = 0x3EB42;

    AddTargetRequest sendMessage;
    sendMessage.TargetId = 0x12;
    sendMessage.SpellId.ModId = 0xFFFFFFFF;
    sendMessage.EffectId.BaseId = 0x3EB42;
    sendMessage.Magnitude = 10.f;
    sendMessage.CustomAlchemy.IsPoison = true;
    sendMessage.CustomAlchemy.Effects = {damageHealth};

    Buffer buff(1000);
    Buffer::Writer writer(&buff);
    sendMessage.Serialize(writer);

    Buffer::Reader reader(&buff);
    const ClientMessageFactory factory;
    auto pMessage = factory.Extract(reader);

    REQUIRE(pMessage);
    REQUIRE(pMessage->GetOpcode() == sendMessage.GetOpcode());
    auto pReceived = CastUnique<AddTargetRequest>(std::move(pMessage));
    REQUIRE(*pReceived == sendMessage);
    REQUIRE(pReceived->CustomAlchemy.IsPoison);
}

TEST_CASE("Player-made potion stacks", "[encoding.inventory]")
{
    Inventory::EffectItem restoreHealth;
    restoreHealth.Magnitude = 37.5f;
    restoreHealth.EffectId.BaseId = 0x3EB15;

    Inventory::Entry potion;
    potion.BaseId.ModId = 0xFFFFFFFF;
    potion.Count = 2;
    potion.CustomAlchemy.Effects = {restoreHealth};

    GIVEN("The same potion from another player")
    {
        Inventory inventory;
        inventory.AddOrRemoveEntry(potion);
        inventory.AddOrRemoveEntry(potion);

        REQUIRE(inventory.Entries.size() == 1);
        REQUIRE(inventory.Entries[0].Count == 4);
    }

    GIVEN("A different potion with the same base id")
    {
        Inventory::Entry poison = potion;
        poison.CustomAlchemy.IsPoison = true;

        Inventory::Entry strongerPotion = potion;
        strongerPotion.CustomAlchemy.Effects[0].Magnitude = 50.f;

        Inventory inventory;
        inventory.AddOrRemoveEntry(potion);
        inventory.AddOrRemoveEntry(poison);
        inventory.AddOrRemoveEntry(strongerPotion);

        REQUIRE(inventory.Entries.size() == 3);
    }

    GIVEN("Taking the potion back out")
    {
        Inventory::Entry taken = potion;
        taken.Count = -2;

        Inventory inventory;
        inventory.AddOrRemoveEntry(potion);
        inventory.AddOrRemoveEntry(taken);

        REQUIRE(inventory.Entries.empty());
    }
}

TEST_CASE("StringCache", "[encoding.string_cache]")
{
    SECTION("Messages")
    {
        StringCacheUpdate update;
        update.Values.push_back("Hello");
        update.Values.push_back("Bye");

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        update.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        StringCacheUpdate recvUpdate;
        recvUpdate.DeserializeRaw(reader);

        REQUIRE(update == recvUpdate);
    }
}
