#include <Services/ReviveService.h>

#include <GameServer.h>
#include <World.h>

#include <Events/PlayerLeaveEvent.h>

#include <Messages/NotifyRevive.h>
#include <Messages/ReviveRequest.h>

ReviveService::ReviveService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_reviveRequestConnection(aDispatcher.sink<PacketEvent<ReviveRequest>>().connect<&ReviveService::OnReviveRequest>(this))
    , m_playerLeaveConnection(aDispatcher.sink<PlayerLeaveEvent>().connect<&ReviveService::OnPlayerLeave>(this))
{
}

void ReviveService::OnReviveRequest(const PacketEvent<ReviveRequest>& acPacket) noexcept
{
    const auto& message = acPacket.Packet;
    const uint32_t cSenderId = acPacket.pPlayer->GetId();

    switch (message.Action)
    {
    case ReviveAction::Downed: SetDowned(cSenderId, true, std::clamp(message.Seconds, 0.f, 600.f)); break;
    case ReviveAction::Up: SetDowned(cSenderId, false); break;
    case ReviveAction::Revive:
    {
        if (message.PlayerId == cSenderId || !m_downed.contains(message.PlayerId))
            return;
        Player* pTarget = m_world.GetPlayerManager().GetById(message.PlayerId);
        if (!pTarget)
            return;

        spdlog::info("[ReviveService]: player {} revives player {} ({} health)", cSenderId, message.PlayerId, message.Health);

        NotifyRevive revived;
        revived.Event = ReviveEvent::Revived;
        revived.PlayerId = cSenderId;
        revived.Health = std::clamp(message.Health, 1.f, 10000.f);
        pTarget->Send(revived);

        SetDowned(message.PlayerId, false);
        break;
    }
    default: break;
    }
}

void ReviveService::OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept
{
    SetDowned(acEvent.pPlayer->GetId(), false);
}

void ReviveService::SetDowned(uint32_t aPlayerId, bool aDowned, float aSeconds) noexcept
{
    const bool cWasDowned = m_downed.contains(aPlayerId);
    if (cWasDowned == aDowned)
        return;

    if (aDowned)
        m_downed.insert(aPlayerId);
    else
        m_downed.erase(aPlayerId);

    NotifyRevive notify;
    notify.Event = aDowned ? ReviveEvent::Downed : ReviveEvent::Up;
    notify.PlayerId = aPlayerId;
    notify.Seconds = aSeconds;
    for (auto pPlayer : m_world.GetPlayerManager())
    {
        if (pPlayer->GetId() != aPlayerId)
            pPlayer->Send(notify);
    }
}
