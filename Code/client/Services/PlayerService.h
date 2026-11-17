#pragma once

struct World;
struct TransportService;

struct UpdateEvent;
struct ConnectedEvent;
struct DisconnectedEvent;
struct ServerSettings;
struct GridCellChangeEvent;
struct CellChangeEvent;
struct PlayerDialogueEvent;
struct PlayerLevelEvent;
struct PartyJoinedEvent;
struct PartyLeftEvent;

struct NotifyPlayerRespawn;

/**
 * @brief Handles logic related to the local player.
 */
struct PlayerService
{
    PlayerService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~PlayerService() noexcept = default;

    TP_NOCOPYMOVE(PlayerService);

    // Another player used a healing potion on the downed local player: get up with that much health (at least a
    // quarter of the maximum). Returns false if the player isn't down.
    bool Revive(float aHealth) noexcept;
    [[nodiscard]] bool IsDowned() const noexcept { return m_downed; }
    // How many times the local player has been revived this session.
    [[nodiscard]] uint32_t GetReviveCount() const noexcept { return m_revives; }

protected:
    void OnUpdate(const UpdateEvent& acEvent) noexcept;
    void OnConnected(const ConnectedEvent& acEvent) noexcept;
    void OnDisconnected(const DisconnectedEvent& acEvent) noexcept;
    void OnServerSettingsReceived(const ServerSettings& acSettings) noexcept;
    void OnNotifyPlayerRespawn(const NotifyPlayerRespawn& acMessage) const noexcept;
    void OnGridCellChangeEvent(const GridCellChangeEvent& acEvent) const noexcept;
    void OnCellChangeEvent(const CellChangeEvent& acEvent) const noexcept;
    void OnPlayerDialogueEvent(const PlayerDialogueEvent& acEvent) const noexcept;
    void OnPlayerLevelEvent(const PlayerLevelEvent& acEvent) const noexcept;
    void OnPartyJoinedEvent(const PartyJoinedEvent& acEvent) noexcept;
    void OnPartyLeftEvent(const PartyLeftEvent& acEvent) noexcept;

private:
    /**
     * @brief Run the respawn timer, and if it hits 0, respawn the player.
     */
    void RunRespawnUpdates() noexcept;
    void RunPostDeathUpdates() noexcept;
    /**
     * @brief Make sure difficulty doesn't get changed while connected
     */
    void RunDifficultyUpdates() const noexcept;
    void RunLevelUpdates() const noexcept;
    void RunBeastFormDetection() const noexcept;

    void ToggleDeathSystem(bool aSet) noexcept;

    World& m_world;
    entt::dispatcher& m_dispatcher;
    TransportService& m_transport;

    std::chrono::steady_clock::time_point m_respawnDeadline;
    // Down (bleedout) and waiting for a revive or the respawn.
    bool m_downed = false;
    bool m_fadeStarted = false;
    // Revived, waiting for the game to stand the player up.
    bool m_reviving = false;
    bool m_reviveMoved = false;
    uint32_t m_revives = 0;
    std::chrono::steady_clock::time_point m_reviveDeadline;
    int32_t m_serverDifficulty = 6;
    int32_t m_previousDifficulty = 6;

    bool m_isDeathSystemEnabled = true;

    bool m_knockdownStart = false;
    std::chrono::steady_clock::time_point m_knockdownDeadline;

    bool m_godmodeStart = false;
    std::chrono::steady_clock::time_point m_godmodeDeadline;

    uint32_t m_cachedMainSpellId = 0;
    uint32_t m_cachedSecondarySpellId = 0;
    uint32_t m_cachedPowerId = 0;

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_connectedConnection;
    entt::scoped_connection m_disconnectedConnection;
    entt::scoped_connection m_settingsConnection;
    entt::scoped_connection m_notifyRespawnConnection;
    entt::scoped_connection m_gridCellChangeConnection;
    entt::scoped_connection m_cellChangeConnection;
    entt::scoped_connection m_playerDialogueConnection;
    entt::scoped_connection m_playerLevelConnection;
    entt::scoped_connection m_partyJoinedConnection;
    entt::scoped_connection m_partyLeftConnection;
};
