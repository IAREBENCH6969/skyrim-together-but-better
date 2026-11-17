#include <Services/MapService.h>
#include <GameServer.h>
#include <World.h>

#include <Events/PlayerJoinEvent.h>
#include <Events/PlayerLeaveEvent.h>
#include <Events/UpdateEvent.h>
#include <Components.h>

#include <Messages/NotifyRemoveWaypoint.h>
#include <Messages/NotifyPlayerPositions.h>
#include <Messages/NotifySetWaypoint.h>
#include <Messages/RequestRemoveWaypoint.h>
#include <Messages/RequestSetWaypoint.h>

MapService::MapService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
{
    m_playerSetWaypointConnection = aDispatcher.sink<PacketEvent<RequestSetWaypoint>>().connect<&MapService::OnSetWaypointRequest>(this);
    m_playerRemoveWaypointConnection = aDispatcher.sink<PacketEvent<RequestRemoveWaypoint>>().connect<&MapService::OnRemoveWaypointRequest>(this);
    m_playerJoinConnection = aDispatcher.sink<PlayerJoinEvent>().connect<&MapService::OnPlayerJoin>(this);
    m_playerLeaveConnection = aDispatcher.sink<PlayerLeaveEvent>().connect<&MapService::OnPlayerLeave>(this);
    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&MapService::OnUpdate>(this);
}

void MapService::OnSetWaypointRequest(const PacketEvent<RequestSetWaypoint>& acMessage) noexcept
{
    const auto& message = acMessage.Packet;
    const uint32_t cPlayerId = acMessage.pPlayer->GetId();
    m_markers[cPlayerId] = {message.Position, message.WorldSpaceFormID};

    NotifySetWaypoint notify{};
    notify.PlayerId = cPlayerId;
    notify.Position = message.Position;
    notify.WorldSpaceFormID = message.WorldSpaceFormID;
    SendToOthers(notify, cPlayerId);
}

void MapService::OnRemoveWaypointRequest(const PacketEvent<RequestRemoveWaypoint>& acMessage) noexcept
{
    const uint32_t cPlayerId = acMessage.pPlayer->GetId();
    if (!m_markers.erase(cPlayerId))
        return;

    NotifyRemoveWaypoint notify{};
    notify.PlayerId = cPlayerId;
    SendToOthers(notify, cPlayerId);
}

void MapService::OnPlayerJoin(const PlayerJoinEvent& acEvent) noexcept
{
    // Show a player who just joined the markers that are already out there.
    for (const auto& [playerId, marker] : m_markers)
    {
        if (playerId == acEvent.pPlayer->GetId())
            continue;
        NotifySetWaypoint notify{};
        notify.PlayerId = playerId;
        notify.Position = marker.Position;
        notify.WorldSpaceFormID = marker.WorldSpaceFormID;
        acEvent.pPlayer->Send(notify);
    }
}

void MapService::OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept
{
    const uint32_t cPlayerId = acEvent.pPlayer->GetId();
    m_lastOutside.erase(cPlayerId);
    if (!m_markers.erase(cPlayerId))
        return;

    NotifyRemoveWaypoint notify{};
    notify.PlayerId = cPlayerId;
    SendToOthers(notify, cPlayerId);
}

void MapService::SendToOthers(const ServerMessage& acMessage, uint32_t aPlayerId) const noexcept
{
    for (auto pPlayer : m_world.GetPlayerManager())
    {
        if (pPlayer->GetId() != aPlayerId)
            pPlayer->Send(acMessage);
    }
}

void MapService::OnUpdate(const UpdateEvent&) noexcept
{
    const uint64_t cNow = GameServer::Get()->GetTick();
    if (cNow - m_lastPositionsTick < 1000)
        return;
    m_lastPositionsTick = cNow;
    SendPlayerPositions();
}

void MapService::SendPlayerPositions() noexcept
{
    NotifyPlayerPositions notify{};
    for (auto pPlayer : m_world.GetPlayerManager())
    {
        const auto cCharacter = pPlayer->GetCharacter();
        if (!cCharacter)
            continue;
        const auto* pMovement = m_world.try_get<MovementComponent>(*cCharacter);
        if (!pMovement)
            continue;

        const uint32_t cPlayerId = pPlayer->GetId();
        NotifyPlayerPositions::Entry entry{};
        entry.PlayerId = cPlayerId;

        const auto& cell = pPlayer->GetCellComponent();
        if (cell.WorldSpaceId)
        {
            Outside& outside = m_lastOutside[cPlayerId];
            outside.WorldSpaceId = cell.WorldSpaceId;
            outside.Position.x = pMovement->Position.x;
            outside.Position.y = pMovement->Position.y;
            outside.Position.z = pMovement->Position.z;
            outside.Heading = pMovement->Rotation.z;
        }
        else
            entry.Inside = true; // in an interior: show where they went in

        auto itor = m_lastOutside.find(cPlayerId);
        if (itor == m_lastOutside.end())
            continue; // never seen outside yet: nowhere to put them on the map
        entry.WorldSpaceId = itor->second.WorldSpaceId;
        entry.Position = itor->second.Position;
        entry.Heading = itor->second.Heading;
        notify.Players.push_back(entry);
    }

    if (notify.Players.empty())
        return;
    for (auto pPlayer : m_world.GetPlayerManager())
        pPlayer->Send(notify);
}
