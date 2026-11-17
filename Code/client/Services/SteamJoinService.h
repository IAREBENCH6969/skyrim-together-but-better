#pragma once

#include <Events/ConnectedEvent.h>
#include <Events/DisconnectedEvent.h>

#include <atomic>
#include <mutex>
#include <thread>

struct World;
struct TransportService;
struct UpdateEvent;
struct BridgeLink;

/**
 * @brief Lets friends join through Steam, without IP addresses or port forwarding.
 *
 * While the local player is connected to a server running on their own PC, this opens a friends-only Steam lobby,
 * so friends see "Join Game" in their Steam friends list (and can be invited from the overlay). A friend who joins
 * connects through a bridge: their client talks UDP to a local socket, and the bridge carries those datagrams over
 * Steam's relay network (ISteamNetworkingMessages) to the host, where they are handed to the server as UDP again.
 * The game's own netcode, encryption and protocol are unchanged.
 *
 * Uses the steam_api64.dll the game already loaded (flat API, resolved at runtime), and registers its callbacks the
 * classic way, so the game's own Steam callbacks keep working.
 */
struct SteamJoinService
{
    SteamJoinService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~SteamJoinService() noexcept;

    TP_NOCOPYMOVE(SteamJoinService);

    [[nodiscard]] bool IsAvailable() const noexcept { return m_available; }

    enum class LoadResult
    {
        Ready,
        NotYet, // the game hasn't loaded or initialised Steam yet
        Failed  // this steam_api64.dll lacks a function we need
    };
    [[nodiscard]] bool IsHosting() const noexcept { return m_hosting; }

    // Called from the Steam callbacks.
    void OnLobbyCreated(uint64_t aLobbyId, bool aSucceeded) noexcept;
    void OnLobbyEntered(uint64_t aLobbyId, bool aSucceeded) noexcept;
    void OnJoinRequested(uint64_t aLobbyId) noexcept;
    void OnSessionRequest(uint64_t aSteamId) noexcept;

    // Test mode (steam_bridge_test.txt next to the exe): the bridge runs over UDP on this PC instead of Steam, so the
    // whole join path can be tested with two clients and one Steam account. Joins the local test host.
    void TestJoin() noexcept;

private:
    void OnUpdate(const UpdateEvent& acEvent) noexcept;
    void OnConnected(const ConnectedEvent& acEvent) noexcept;
    void OnDisconnected(const DisconnectedEvent& acEvent) noexcept;

    void StartHosting(uint16_t aServerPort) noexcept;
    void StartJoining(uint64_t aHostId) noexcept;
    void Stop() noexcept;

    void BridgeLoop() noexcept;
    void WaitForSteam() noexcept;

    World& m_world;
    TransportService& m_transport;

    std::atomic<bool> m_available{};
    std::thread m_initThread;
    std::atomic<bool> m_stopInit{};
    bool m_testMode{};
    std::atomic<bool> m_hosting{};
    std::atomic<bool> m_joining{};
    uint64_t m_lobbyId{};
    uint64_t m_pendingJoinLobby{};
    uint16_t m_serverPort{};
    uint16_t m_bridgePort{};
    bool m_connectPending{};

    std::unique_ptr<BridgeLink> m_pLink;
    std::thread m_bridgeThread;
    std::atomic<bool> m_stopBridge{};
    std::mutex m_peersLock;
    uint64_t m_hostId{};

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_connectedConnection;
    entt::scoped_connection m_disconnectedConnection;
};
