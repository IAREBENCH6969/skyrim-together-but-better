#pragma once

struct World;
struct TransportService;
struct UpdateEvent;
struct DisconnectedEvent;
struct ServerSettings;
struct NotifyRevive;
struct TESObjectREFR;
struct TESBoundObject;

/**
 * @brief Reviving downed players with healing potions.
 *
 * A player whose health reaches 0 goes down (bleedout) for the server's revive window instead of respawning right
 * away (see PlayerService). Meanwhile another player can walk up to them and activate them: one of the reviver's
 * healing potions (anything that restores health, player-made ones included) is used up and the downed player gets
 * up with that much health. If nobody comes, they respawn as before.
 */
struct ReviveService
{
    ReviveService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~ReviveService() noexcept = default;

    TP_NOCOPYMOVE(ReviveService);

    // Seconds a downed player can be revived (from the server settings); 0 when reviving is off.
    [[nodiscard]] float GetReviveWindow() const noexcept { return m_reviveWindow; }
    [[nodiscard]] bool IsDowned(uint32_t aPlayerId) const noexcept { return m_downedPlayers.contains(aPlayerId); }
    // Whether any other player is in the game with us (only then is it worth waiting for a revive).
    [[nodiscard]] bool HasOtherPlayers() const noexcept;

    // From PlayerService: the local player went down / is up again.
    void ReportDowned(bool aDowned) noexcept;

    // The local player pressed Activate (E): revive the nearest downed player in reach and in front of them. Returns true
    // if there was one (the press is then used up by the revive attempt).
    bool TryReviveNearby() noexcept;

    // Revive this downed player with one of the local player's healing potions. Returns true if apTarget is a downed
    // player (whether or not a potion was found).
    bool TryRevive(TESObjectREFR* apTarget) noexcept;

    // The weakest healing potion the local player carries, and how much health it restores; nullptr if none.
    static TESBoundObject* FindHealingPotion(float& aHealth) noexcept;

private:
    void OnUpdate(const UpdateEvent&) noexcept;
    void OnDisconnected(const DisconnectedEvent&) noexcept;
    void OnServerSettings(const ServerSettings& acSettings) noexcept;
    void OnNotifyRevive(const NotifyRevive& acMessage) noexcept;

    uint32_t PlayerIdOf(TESObjectREFR* apActor) const noexcept;
    String NameOf(uint32_t aPlayerId) const noexcept;

    World& m_world;
    TransportService& m_transport;

    float m_reviveWindow{};
    TiltedPhoques::Set<uint32_t> m_downedPlayers;
    TiltedPhoques::Map<uint32_t, std::chrono::steady_clock::time_point> m_nextHint;
    std::chrono::steady_clock::time_point m_nextUpdate{};
    bool m_activateWasDown{};

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_disconnectedConnection;
    entt::scoped_connection m_settingsConnection;
    entt::scoped_connection m_notifyConnection;
};
