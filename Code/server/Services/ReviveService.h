#pragma once

#include <Events/PacketEvent.h>

struct World;
struct PlayerLeaveEvent;
struct ReviveRequest;

/**
 * @brief Tracks which players are downed, and relays revives with healing potions between players.
 *
 * Each client runs its own downed timer (the server's fReviveWindow); this only relays who is down so the others can
 * revive them, and checks that a revive targets a player who really is down.
 */
struct ReviveService
{
    ReviveService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~ReviveService() noexcept = default;

    TP_NOCOPYMOVE(ReviveService);

private:
    void OnReviveRequest(const PacketEvent<ReviveRequest>& acPacket) noexcept;
    void OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept;

    void SetDowned(uint32_t aPlayerId, bool aDowned) noexcept;

    World& m_world;
    TiltedPhoques::Set<uint32_t> m_downed;

    entt::scoped_connection m_reviveRequestConnection;
    entt::scoped_connection m_playerLeaveConnection;
};
