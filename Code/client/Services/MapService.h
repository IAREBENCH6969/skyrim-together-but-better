#pragma once

struct World;
struct TransportService;

struct SetWaypointEvent;
struct RemoveWaypointEvent;
struct DisconnectedEvent;
struct NotifySetWaypoint;
struct NotifyRemoveWaypoint;
struct NotifyPlayerPositions;

/**
 * @brief Shares custom map markers between players.
 *
 * The local player's custom marker is sent to the server; every other player's marker is kept here and drawn on top of
 * the game (it doesn't replace the local player's own marker): on the compass at its bearing, and on the world map at
 * its position, each in that player's colour with their name.
 *
 * The world map also shows where the other players are (the server sends everyone's position about once a second),
 * as an arrow facing the way they face, with their name. Someone in an interior shows where they went in.
 */
struct MapService
{
    MapService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~MapService() noexcept = default;

    TP_NOCOPYMOVE(MapService);

    struct Marker
    {
        NiPoint3 Position{};
        uint32_t WorldSpaceId{}; // a worldspace, or the cell for an interior marker
    };

    struct PlayerOnMap
    {
        NiPoint3 Position{};
        uint32_t WorldSpaceId{};
        float Heading{}; // radians, clockwise from north
        bool Inside{};   // in an interior; Position is where they went in
    };

    [[nodiscard]] const TiltedPhoques::Map<uint32_t, Marker>& GetMarkers() const noexcept { return m_markers; }
    [[nodiscard]] const TiltedPhoques::Map<uint32_t, PlayerOnMap>& GetPlayersOnMap() const noexcept { return m_players; }

  protected:
    void OnSetWaypoint(const SetWaypointEvent& acMessage) noexcept;
    void OnRemoveWaypoint(const RemoveWaypointEvent& acMessage) noexcept;
    void OnNotifySetWaypoint(const NotifySetWaypoint& acMessage) noexcept;
    void OnNotifyRemoveWaypoint(const NotifyRemoveWaypoint& acMessage) noexcept;
    void OnNotifyPlayerPositions(const NotifyPlayerPositions& acMessage) noexcept;
    void OnDisconnected(const DisconnectedEvent&) noexcept;
    void OnDraw() noexcept;

  private:
    void DrawOnCompass() noexcept;
    void DrawOnMap() noexcept;

    World& m_world;
    entt::dispatcher& m_dispatcher;
    TransportService& m_transport;

    TiltedPhoques::Map<uint32_t, Marker> m_markers;
    TiltedPhoques::Map<uint32_t, PlayerOnMap> m_players;

    entt::scoped_connection m_playerSetWaypointConnection;
    entt::scoped_connection m_playerRemoveWaypointConnection;
    entt::scoped_connection m_playerNotifySetWaypointConnection;
    entt::scoped_connection m_playerNotifyRemoveWaypointConnection;
    entt::scoped_connection m_playerPositionsConnection;
    entt::scoped_connection m_disconnectedConnection;
    entt::scoped_connection m_drawConnection;
};
