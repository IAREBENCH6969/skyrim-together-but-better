#include <TiltedOnlinePCH.h>

#include <Services/SteamJoinService.h>
#include <Services/TransportService.h>
#include <World.h>
#include <Events/UpdateEvent.h>

#include <PlayerCharacter.h>

#include <cstdlib>
#include <string_view>
#include <fstream>
#include <functional>
#include <unordered_map>

#pragma comment(lib, "ws2_32.lib")

namespace
{
// ---- Steamworks flat API, resolved from the game's own steam_api64.dll -------------------------------------------

// Mirrors Steamworks' CCallbackBase (same declaration order, so MSVC lays out the same vtable).
class SteamCallbackBase
{
public:
    SteamCallbackBase() noexcept = default;
    virtual void Run(void* apParam) = 0;
    virtual void Run(void* apParam, bool aIoFailure, uint64_t aApiCall) = 0;
    virtual int GetCallbackSizeBytes() = 0;

protected:
    uint8_t m_callbackFlags{};
    int m_callbackId{};
};

// Opaque SteamNetworkingIdentity (136 bytes); only read and written through the API.
struct alignas(8) SteamIdentity
{
    uint8_t data[136]{};
};

// The fields of SteamNetworkingMessage_t that are used here.
struct SteamMessage
{
    void* pData;
    int size;
    uint32_t connection;
    SteamIdentity peer;
};
static_assert(offsetof(SteamMessage, peer) == 16);

constexpr int kLobbyFriendsOnly = 1;
constexpr int kMaxLobbyMembers = 8;
constexpr int kSendUnreliableNoNagleAutoRestart = 1 | 32;
constexpr int kChannel = 0;

constexpr int kCallbackGameLobbyJoinRequested = 333;
constexpr int kCallbackGameRichPresenceJoinRequested = 337;
constexpr int kCallbackLobbyEnter = 504;
constexpr int kCallbackLobbyCreated = 513;
constexpr int kCallbackSessionRequest = 1251;

struct SteamApi
{
    void* (*Matchmaking)(){};
    void* (*Friends)(){};
    void* (*User)(){};
    void* (*Messages)(){};
    void* (*Utils)(){};
    void (*RegisterCallback)(SteamCallbackBase*, int){};
    void (*UnregisterCallback)(SteamCallbackBase*){};
    uint64_t (*CreateLobby)(void*, int, int){};
    uint64_t (*JoinLobby)(void*, uint64_t){};
    void (*LeaveLobby)(void*, uint64_t){};
    bool (*SetLobbyData)(void*, uint64_t, const char*, const char*){};
    uint64_t (*GetLobbyOwner)(void*, uint64_t){};
    int (*GetNumLobbyMembers)(void*, uint64_t){};
    uint64_t (*GetLobbyMemberByIndex)(void*, uint64_t, int){};
    bool (*SetRichPresence)(void*, const char*, const char*){};
    void (*ClearRichPresence)(void*){};
    uint64_t (*GetSteamId)(void*){};
    int (*SendMessageToUser)(void*, const SteamIdentity*, const void*, uint32_t, int, int){};
    int (*ReceiveMessagesOnChannel)(void*, int, SteamMessage**, int){};
    bool (*AcceptSessionWithUser)(void*, const SteamIdentity*){};
    bool (*CloseSessionWithUser)(void*, const SteamIdentity*){};
    void (*SetSteamId64)(SteamIdentity*, uint64_t){};
    uint64_t (*GetSteamId64)(SteamIdentity*){};
    void (*ReleaseMessage)(SteamMessage*){};
    void (*InitRelayNetworkAccess)(void*){};

    void* pMatchmaking{};
    void* pFriends{};
    void* pUser{};
    void* pMessages{};

    SteamJoinService::LoadResult Load() noexcept
    {
        HMODULE hSteam = GetModuleHandleW(L"steam_api64.dll");
        if (!hSteam)
            return SteamJoinService::LoadResult::NotYet;

        bool ok = true;
        auto resolve = [&](auto& aFunction, const char* acpName)
        {
            aFunction = reinterpret_cast<std::remove_reference_t<decltype(aFunction)>>(GetProcAddress(hSteam, acpName));
            if (!aFunction)
            {
                spdlog::warn("Steam join: steam_api64.dll has no {}", acpName);
                ok = false;
            }
        };
        resolve(Matchmaking, "SteamAPI_SteamMatchmaking_v009");
        resolve(Friends, "SteamAPI_SteamFriends_v017");
        resolve(User, "SteamAPI_SteamUser_v021");
        resolve(Messages, "SteamAPI_SteamNetworkingMessages_SteamAPI_v002");
        resolve(Utils, "SteamAPI_SteamNetworkingUtils_SteamAPI_v004");
        resolve(RegisterCallback, "SteamAPI_RegisterCallback");
        resolve(UnregisterCallback, "SteamAPI_UnregisterCallback");
        resolve(CreateLobby, "SteamAPI_ISteamMatchmaking_CreateLobby");
        resolve(JoinLobby, "SteamAPI_ISteamMatchmaking_JoinLobby");
        resolve(LeaveLobby, "SteamAPI_ISteamMatchmaking_LeaveLobby");
        resolve(SetLobbyData, "SteamAPI_ISteamMatchmaking_SetLobbyData");
        resolve(GetLobbyOwner, "SteamAPI_ISteamMatchmaking_GetLobbyOwner");
        resolve(GetNumLobbyMembers, "SteamAPI_ISteamMatchmaking_GetNumLobbyMembers");
        resolve(GetLobbyMemberByIndex, "SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex");
        resolve(SetRichPresence, "SteamAPI_ISteamFriends_SetRichPresence");
        resolve(ClearRichPresence, "SteamAPI_ISteamFriends_ClearRichPresence");
        resolve(GetSteamId, "SteamAPI_ISteamUser_GetSteamID");
        resolve(SendMessageToUser, "SteamAPI_ISteamNetworkingMessages_SendMessageToUser");
        resolve(ReceiveMessagesOnChannel, "SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel");
        resolve(AcceptSessionWithUser, "SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser");
        resolve(CloseSessionWithUser, "SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser");
        resolve(SetSteamId64, "SteamAPI_SteamNetworkingIdentity_SetSteamID64");
        resolve(GetSteamId64, "SteamAPI_SteamNetworkingIdentity_GetSteamID64");
        resolve(ReleaseMessage, "SteamAPI_SteamNetworkingMessage_t_Release");
        resolve(InitRelayNetworkAccess, "SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess");
        if (!ok)
            return SteamJoinService::LoadResult::Failed;

        pMatchmaking = Matchmaking();
        pFriends = Friends();
        pUser = User();
        pMessages = Messages();
        if (!pMatchmaking || !pFriends || !pUser || !pMessages)
            return SteamJoinService::LoadResult::NotYet;

        if (void* pUtils = Utils())
            InitRelayNetworkAccess(pUtils);
        return SteamJoinService::LoadResult::Ready;
    }

    SteamIdentity Identity(uint64_t aSteamId) noexcept
    {
        SteamIdentity identity{};
        SetSteamId64(&identity, aSteamId);
        return identity;
    }
};

SteamApi g_steam{};
SteamJoinService* g_pService{};

template <int TId, size_t TSize, class TFunction> class SteamCallback final : public SteamCallbackBase
{
public:
    explicit SteamCallback(TFunction aFunction) noexcept
        : m_function(aFunction)
    {
        m_callbackId = TId;
    }
    void Run(void* apParam) override { m_function(apParam); }
    void Run(void* apParam, bool, uint64_t) override { m_function(apParam); }
    int GetCallbackSizeBytes() override { return static_cast<int>(TSize); }

private:
    TFunction m_function;
};

void OnGameLobbyJoinRequested(void* apParam)
{
    // GameLobbyJoinRequested_t { CSteamID m_steamIDLobby; CSteamID m_steamIDFriend; }
    if (g_pService)
        g_pService->OnJoinRequested(*static_cast<uint64_t*>(apParam));
}
void OnRichPresenceJoinRequested(void* apParam)
{
    // GameRichPresenceJoinRequested_t { CSteamID m_steamIDFriend; char m_rgchConnect[256]; }: "+connect_lobby <id>"
    const char* cpConnect = static_cast<const char*>(apParam) + 8;
    constexpr std::string_view cPrefix = "+connect_lobby ";
    if (g_pService && std::string_view(cpConnect).substr(0, cPrefix.size()) == cPrefix)
    {
        if (const uint64_t cLobbyId = std::strtoull(cpConnect + cPrefix.size(), nullptr, 10))
            g_pService->OnJoinRequested(cLobbyId);
    }
}
void OnLobbyEnter(void* apParam)
{
    // LobbyEnter_t { uint64 m_ulSteamIDLobby; uint32 m_rgfChatPermissions; bool m_bLocked; uint32 m_EChatRoomEnterResponse; }
    const auto* pBytes = static_cast<const uint8_t*>(apParam);
    if (g_pService)
        g_pService->OnLobbyEntered(*reinterpret_cast<const uint64_t*>(pBytes), *reinterpret_cast<const uint32_t*>(pBytes + 16) == 1);
}
void OnLobbyCreated(void* apParam)
{
    // LobbyCreated_t { EResult m_eResult; uint64 m_ulSteamIDLobby; }
    const auto* pBytes = static_cast<const uint8_t*>(apParam);
    if (g_pService)
        g_pService->OnLobbyCreated(*reinterpret_cast<const uint64_t*>(pBytes + 8), *reinterpret_cast<const int32_t*>(pBytes) == 1);
}
void OnSessionRequest(void* apParam)
{
    // SteamNetworkingMessagesSessionRequest_t { SteamNetworkingIdentity m_identityRemote; }
    if (g_pService)
        g_pService->OnSessionRequest(g_steam.GetSteamId64(static_cast<SteamIdentity*>(apParam)));
}

SteamCallback<kCallbackGameLobbyJoinRequested, 16, void (*)(void*)> s_joinRequested{&OnGameLobbyJoinRequested};
SteamCallback<kCallbackGameRichPresenceJoinRequested, 264, void (*)(void*)> s_richPresenceJoin{&OnRichPresenceJoinRequested};
SteamCallback<kCallbackLobbyEnter, 24, void (*)(void*)> s_lobbyEnter{&OnLobbyEnter};
SteamCallback<kCallbackLobbyCreated, 16, void (*)(void*)> s_lobbyCreated{&OnLobbyCreated};
SteamCallback<kCallbackSessionRequest, 136, void (*)(void*)> s_sessionRequest{&OnSessionRequest};

bool IsLoopbackEndpoint(const std::string& acEndpoint, uint16_t& aPort) noexcept
{
    const auto cColon = acEndpoint.rfind(':');
    const std::string cHost = cColon == std::string::npos ? acEndpoint : acEndpoint.substr(0, cColon);
    aPort = 10578;
    if (cColon != std::string::npos)
        aPort = static_cast<uint16_t>(std::strtoul(acEndpoint.c_str() + cColon + 1, nullptr, 10));
    return cHost == "127.0.0.1" || cHost == "localhost" || cHost == "[::1]";
}

SOCKET OpenLoopbackSocket(uint16_t aPort) noexcept
{
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
        return sock;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(aPort);
    u_long nonBlocking = 1;
    if (bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || ioctlsocket(sock, FIONBIO, &nonBlocking) != 0)
    {
        closesocket(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

uint16_t LocalPort(SOCKET aSocket) noexcept
{
    sockaddr_in address{};
    int length = sizeof(address);
    getsockname(aSocket, reinterpret_cast<sockaddr*>(&address), &length);
    return ntohs(address.sin_port);
}

sockaddr_in LoopbackAddress(uint16_t aPort) noexcept
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(aPort);
    return address;
}

constexpr uint16_t kTestRelayPort = 10700;
} // namespace

// ---- Links: what carries datagrams between the host's and the joiner's bridge ------------------------------------

struct BridgeLink
{
    virtual ~BridgeLink() = default;
    virtual void Send(uint64_t aPeer, const void* apData, uint32_t aSize) noexcept = 0;
    // Calls aReceive(peer, data, size) for each waiting datagram; returns how many there were.
    virtual int Poll(const std::function<void(uint64_t, const uint8_t*, uint32_t)>& aReceive) noexcept = 0;
};

namespace
{
// Steam's relay network: peers are SteamIDs.
struct SteamLink final : BridgeLink
{
    void Send(uint64_t aPeer, const void* apData, uint32_t aSize) noexcept override
    {
        const SteamIdentity cIdentity = g_steam.Identity(aPeer);
        g_steam.SendMessageToUser(g_steam.pMessages, &cIdentity, apData, aSize, kSendUnreliableNoNagleAutoRestart, kChannel);
    }

    int Poll(const std::function<void(uint64_t, const uint8_t*, uint32_t)>& aReceive) noexcept override
    {
        SteamMessage* messages[64]{};
        const int cCount = g_steam.ReceiveMessagesOnChannel(g_steam.pMessages, kChannel, messages, 64);
        for (int i = 0; i < cCount; i++)
        {
            aReceive(g_steam.GetSteamId64(&messages[i]->peer), static_cast<const uint8_t*>(messages[i]->pData), static_cast<uint32_t>(messages[i]->size));
            g_steam.ReleaseMessage(messages[i]);
        }
        return cCount;
    }
};

// Test stand-in for Steam on one PC: the host's bridge listens on 127.0.0.1:10700; a peer is its UDP port.
struct UdpLink final : BridgeLink
{
    explicit UdpLink(bool aHost) noexcept
        : m_socket(OpenLoopbackSocket(aHost ? kTestRelayPort : 0))
    {
    }
    ~UdpLink() override
    {
        if (m_socket != INVALID_SOCKET)
            closesocket(m_socket);
    }

    void Send(uint64_t aPeer, const void* apData, uint32_t aSize) noexcept override
    {
        const sockaddr_in cTo = LoopbackAddress(static_cast<uint16_t>(aPeer));
        sendto(m_socket, static_cast<const char*>(apData), static_cast<int>(aSize), 0, reinterpret_cast<const sockaddr*>(&cTo), sizeof(cTo));
    }

    int Poll(const std::function<void(uint64_t, const uint8_t*, uint32_t)>& aReceive) noexcept override
    {
        int count = 0;
        uint8_t buffer[2048];
        for (;;)
        {
            sockaddr_in from{};
            int fromLength = sizeof(from);
            const int cSize = recvfrom(m_socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &fromLength);
            if (cSize <= 0)
                return count;
            aReceive(ntohs(from.sin_port), buffer, static_cast<uint32_t>(cSize));
            count++;
        }
    }

    SOCKET m_socket;
};

// Bridge state shared with the bridge thread (guarded by SteamJoinService::m_peersLock).
struct HostPeer
{
    SOCKET socket{INVALID_SOCKET}; // connected to the local server, one per remote player
};
std::unordered_map<uint64_t, HostPeer> g_hostPeers;
SOCKET g_joinSocket{INVALID_SOCKET}; // joiner: the game client talks to this
sockaddr_in g_joinClient{};          // joiner: where the game client sent from
bool g_joinClientKnown{};
} // namespace

SteamJoinService::SteamJoinService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_transport(aTransport)
{
    g_pService = this;

    WSADATA wsaData{};
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    m_testMode = std::ifstream(TiltedPhoques::GetPath() / "steam_bridge_test.txt").good();
    if (m_testMode)
    {
        m_available = true;
        spdlog::info("Steam join: test mode, the bridge runs over UDP on this PC");
    }
    else
        m_initThread = std::thread([this] { WaitForSteam(); });

    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&SteamJoinService::OnUpdate>(this);
    m_connectedConnection = aDispatcher.sink<ConnectedEvent>().connect<&SteamJoinService::OnConnected>(this);
    m_disconnectedConnection = aDispatcher.sink<DisconnectedEvent>().connect<&SteamJoinService::OnDisconnected>(this);
}

// The client starts before the game loads and initialises Steam, and a friend's "Join Game" can arrive at the main
// menu, so wait for Steam here rather than in the per-frame update (which only runs in game).
void SteamJoinService::WaitForSteam() noexcept
{
    while (!m_stopInit)
    {
        switch (g_steam.Load())
        {
        case LoadResult::Ready:
            g_steam.RegisterCallback(&s_joinRequested, kCallbackGameLobbyJoinRequested);
            g_steam.RegisterCallback(&s_richPresenceJoin, kCallbackGameRichPresenceJoinRequested);
            g_steam.RegisterCallback(&s_lobbyEnter, kCallbackLobbyEnter);
            g_steam.RegisterCallback(&s_lobbyCreated, kCallbackLobbyCreated);
            g_steam.RegisterCallback(&s_sessionRequest, kCallbackSessionRequest);
            m_available = true;
            spdlog::info("Steam join: ready");
            return;
        case LoadResult::Failed: spdlog::warn("Steam join: unavailable, the game's steam_api64.dll lacks functions it needs"); return;
        case LoadResult::NotYet: Sleep(500); break;
        }
    }
}

SteamJoinService::~SteamJoinService() noexcept
{
    m_stopInit = true;
    if (m_initThread.joinable())
        m_initThread.join();
    Stop();
    if (m_available && !m_testMode)
    {
        g_steam.UnregisterCallback(&s_joinRequested);
        g_steam.UnregisterCallback(&s_richPresenceJoin);
        g_steam.UnregisterCallback(&s_lobbyEnter);
        g_steam.UnregisterCallback(&s_lobbyCreated);
        g_steam.UnregisterCallback(&s_sessionRequest);
    }
    g_pService = nullptr;
}

void SteamJoinService::OnConnected(const ConnectedEvent&) noexcept
{
    uint16_t port = 0;
    if (!m_available || m_joining || m_hosting || !IsLoopbackEndpoint(m_transport.GetEndpoint(), port))
        return;

    // Connected to a server on this PC: this player hosts, so friends may join through Steam.
    StartHosting(port);
}

void SteamJoinService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    if (m_hosting || m_joining)
        spdlog::info("Steam join: disconnected, closing the {}", m_hosting ? "lobby" : "bridge");
    Stop();
}

void SteamJoinService::OnUpdate(const UpdateEvent&) noexcept
{
    // A friend joins from the main menu or a loading screen: connect once the game runs.
    if (m_connectPending && PlayerCharacter::Get() && PlayerCharacter::Get()->GetParentCellEx())
    {
        m_connectPending = false;
        const std::string cEndpoint = "127.0.0.1:" + std::to_string(m_bridgePort);
        spdlog::info("Steam join: connecting through the bridge at {}", cEndpoint);
        m_transport.Connect(cEndpoint);
    }
}

void SteamJoinService::StartHosting(uint16_t aServerPort) noexcept
{
    m_serverPort = aServerPort;
    m_hosting = true;

    if (m_testMode)
    {
        m_pLink = std::make_unique<UdpLink>(true);
        spdlog::info("Steam join: hosting (test relay on 127.0.0.1:{}, server port {})", kTestRelayPort, aServerPort);
    }
    else
    {
        m_pLink = std::make_unique<SteamLink>();
        g_steam.CreateLobby(g_steam.pMatchmaking, kLobbyFriendsOnly, kMaxLobbyMembers);
        spdlog::info("Steam join: hosting, creating a friends-only lobby (server port {})", aServerPort);
    }

    m_stopBridge = false;
    m_bridgeThread = std::thread([this] { BridgeLoop(); });
}

void SteamJoinService::OnLobbyCreated(uint64_t aLobbyId, bool aSucceeded) noexcept
{
    if (!m_hosting)
        return;
    if (!aSucceeded)
    {
        spdlog::error("Steam join: could not create the lobby");
        return;
    }

    m_lobbyId = aLobbyId;
    g_steam.SetLobbyData(g_steam.pMatchmaking, aLobbyId, "skyrimtogether", "1");
    const std::string cConnect = "+connect_lobby " + std::to_string(aLobbyId);
    g_steam.SetRichPresence(g_steam.pFriends, "connect", cConnect.c_str());
    g_steam.SetRichPresence(g_steam.pFriends, "status", "Hosting Skyrim Together");
    spdlog::info("Steam join: lobby {} open, friends can use Join Game", aLobbyId);
}

void SteamJoinService::OnJoinRequested(uint64_t aLobbyId) noexcept
{
    if (m_hosting)
    {
        spdlog::warn("Steam join: ignoring a join request while hosting");
        return;
    }
    spdlog::info("Steam join: joining lobby {}", aLobbyId);
    m_pendingJoinLobby = aLobbyId;
    g_steam.JoinLobby(g_steam.pMatchmaking, aLobbyId);
}

void SteamJoinService::OnLobbyEntered(uint64_t aLobbyId, bool aSucceeded) noexcept
{
    if (m_hosting || aLobbyId != m_pendingJoinLobby)
        return;
    if (!aSucceeded)
    {
        spdlog::error("Steam join: could not enter lobby {}", aLobbyId);
        return;
    }

    m_lobbyId = aLobbyId;
    const uint64_t cHost = g_steam.GetLobbyOwner(g_steam.pMatchmaking, aLobbyId);
    if (cHost == g_steam.GetSteamId(g_steam.pUser))
    {
        spdlog::warn("Steam join: this is our own lobby");
        return;
    }
    StartJoining(cHost);
}

void SteamJoinService::TestJoin() noexcept
{
    if (!m_testMode)
        return;
    StartJoining(kTestRelayPort);
}

void SteamJoinService::StartJoining(uint64_t aHostId) noexcept
{
    g_joinSocket = OpenLoopbackSocket(0);
    if (g_joinSocket == INVALID_SOCKET)
    {
        spdlog::error("Steam join: could not open the local bridge socket");
        return;
    }
    g_joinClientKnown = false;
    m_bridgePort = LocalPort(g_joinSocket);
    m_hostId = aHostId;
    m_joining = true;
    m_pLink = m_testMode ? std::unique_ptr<BridgeLink>(std::make_unique<UdpLink>(false)) : std::unique_ptr<BridgeLink>(std::make_unique<SteamLink>());

    m_stopBridge = false;
    m_bridgeThread = std::thread([this] { BridgeLoop(); });

    spdlog::info("Steam join: bridge to host {} on 127.0.0.1:{}", aHostId, m_bridgePort);
    m_connectPending = true;
}

void SteamJoinService::OnSessionRequest(uint64_t aSteamId) noexcept
{
    if (!m_hosting || !m_lobbyId)
        return;

    // Only players who are in our friends-only lobby may open a session.
    const int cMembers = g_steam.GetNumLobbyMembers(g_steam.pMatchmaking, m_lobbyId);
    for (int i = 0; i < cMembers; i++)
    {
        if (g_steam.GetLobbyMemberByIndex(g_steam.pMatchmaking, m_lobbyId, i) == aSteamId)
        {
            const SteamIdentity cIdentity = g_steam.Identity(aSteamId);
            g_steam.AcceptSessionWithUser(g_steam.pMessages, &cIdentity);
            spdlog::info("Steam join: accepted {}", aSteamId);
            return;
        }
    }
    spdlog::warn("Steam join: refused a session from {}, not in the lobby", aSteamId);
}

void SteamJoinService::Stop() noexcept
{
    m_stopBridge = true;
    if (m_bridgeThread.joinable())
        m_bridgeThread.join();

    {
        std::scoped_lock _(m_peersLock);
        for (auto& [steamId, peer] : g_hostPeers)
        {
            closesocket(peer.socket);
            if (!m_testMode && m_available)
            {
                const SteamIdentity cIdentity = g_steam.Identity(steamId);
                g_steam.CloseSessionWithUser(g_steam.pMessages, &cIdentity);
            }
        }
        g_hostPeers.clear();
        if (g_joinSocket != INVALID_SOCKET)
            closesocket(g_joinSocket);
        g_joinSocket = INVALID_SOCKET;
    }
    m_pLink.reset();

    if (m_lobbyId && !m_testMode)
    {
        g_steam.LeaveLobby(g_steam.pMatchmaking, m_lobbyId);
        g_steam.ClearRichPresence(g_steam.pFriends);
    }
    m_lobbyId = 0;
    m_hosting = false;
    m_joining = false;
    m_connectPending = false;
}

// Moves datagrams between the link (Steam or the test relay) and the local UDP sockets, until Stop().
void SteamJoinService::BridgeLoop() noexcept
{
    uint8_t buffer[2048];
    while (!m_stopBridge)
    {
        int moved = 0;
        {
            std::scoped_lock _(m_peersLock);

            if (m_hosting)
            {
                // Remote players -> local server, one UDP socket per player so the server tells them apart.
                moved += m_pLink->Poll(
                    [this](uint64_t aPeer, const uint8_t* apData, uint32_t aSize)
                    {
                        auto it = g_hostPeers.find(aPeer);
                        if (it == g_hostPeers.end())
                        {
                            HostPeer peer{OpenLoopbackSocket(0)};
                            if (peer.socket == INVALID_SOCKET)
                                return;
                            const sockaddr_in cServer = LoopbackAddress(m_serverPort);
                            connect(peer.socket, reinterpret_cast<const sockaddr*>(&cServer), sizeof(cServer));
                            it = g_hostPeers.emplace(aPeer, peer).first;
                            spdlog::info("Steam join: player {} connected through the bridge", aPeer);
                        }
                        send(it->second.socket, reinterpret_cast<const char*>(apData), static_cast<int>(aSize), 0);
                    });

                // Local server -> remote players.
                for (auto& [peerId, peer] : g_hostPeers)
                {
                    for (;;)
                    {
                        const int cSize = recv(peer.socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);
                        if (cSize <= 0)
                            break;
                        m_pLink->Send(peerId, buffer, static_cast<uint32_t>(cSize));
                        moved++;
                    }
                }
            }
            else if (m_joining && g_joinSocket != INVALID_SOCKET)
            {
                // Our game client -> host.
                for (;;)
                {
                    sockaddr_in from{};
                    int fromLength = sizeof(from);
                    const int cSize = recvfrom(g_joinSocket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &fromLength);
                    if (cSize <= 0)
                        break;
                    g_joinClient = from;
                    g_joinClientKnown = true;
                    m_pLink->Send(m_hostId, buffer, static_cast<uint32_t>(cSize));
                    moved++;
                }

                // Host -> our game client.
                moved += m_pLink->Poll(
                    [this](uint64_t aPeer, const uint8_t* apData, uint32_t aSize)
                    {
                        if (aPeer != m_hostId || !g_joinClientKnown)
                            return;
                        sendto(
                            g_joinSocket, reinterpret_cast<const char*>(apData), static_cast<int>(aSize), 0, reinterpret_cast<const sockaddr*>(&g_joinClient),
                            sizeof(g_joinClient));
                    });
            }
        }

        if (moved == 0)
            Sleep(1);
    }
}
