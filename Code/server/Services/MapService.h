#pragma once

#include <Events/PacketEvent.h>
#include <Structs/GameId.h>
#include <Structs/Vector3_NetQuantize.h>

struct World;
struct PlayerJoinEvent;
struct PlayerLeaveEvent;
struct RequestSetWaypoint;
struct RequestRemoveWaypoint;

/**
 * @brief Shares every player's custom map marker with the other players.
 *
 * Each player keeps their own marker; the others see it (with the player's name) on their compass and world map,
 * next to their own. The server remembers the current markers so players who join later see them too.
 */
struct MapService
{
    MapService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~MapService() noexcept = default;

    TP_NOCOPYMOVE(MapService);

  protected:
    void OnSetWaypointRequest(const PacketEvent<RequestSetWaypoint>& acMessage) noexcept;
    void OnRemoveWaypointRequest(const PacketEvent<RequestRemoveWaypoint>& acMessage) noexcept;
    void OnPlayerJoin(const PlayerJoinEvent& acEvent) noexcept;
    void OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept;

  private:
    struct Marker
    {
        Vector3_NetQuantize Position;
        GameId WorldSpaceFormID;
    };

    void SendToOthers(const ServerMessage& acMessage, uint32_t aPlayerId) const noexcept;

    World& m_world;
    TiltedPhoques::Map<uint32_t, Marker> m_markers;

    entt::scoped_connection m_playerSetWaypointConnection;
    entt::scoped_connection m_playerRemoveWaypointConnection;
    entt::scoped_connection m_playerJoinConnection;
    entt::scoped_connection m_playerLeaveConnection;
};
