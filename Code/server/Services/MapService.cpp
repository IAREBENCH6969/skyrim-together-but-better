#include <Services/MapService.h>
#include <GameServer.h>
#include <World.h>

#include <Events/PlayerJoinEvent.h>
#include <Events/PlayerLeaveEvent.h>

#include <Messages/NotifyRemoveWaypoint.h>
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
