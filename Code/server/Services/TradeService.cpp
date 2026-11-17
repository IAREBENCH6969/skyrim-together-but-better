#include <Services/TradeService.h>

#include <GameServer.h>
#include <World.h>

#include <Events/PlayerLeaveEvent.h>
#include <Events/UpdateEvent.h>

#include <Messages/NotifyTrade.h>
#include <Messages/TradeRequest.h>

namespace
{
constexpr uint64_t kInvitationLifetimeMs = 60000;
constexpr size_t kMaxOfferEntries = 256;
} // namespace

TradeService::TradeService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_updateConnection(aDispatcher.sink<UpdateEvent>().connect<&TradeService::OnUpdate>(this))
    , m_playerLeaveConnection(aDispatcher.sink<PlayerLeaveEvent>().connect<&TradeService::OnPlayerLeave>(this))
    , m_tradeRequestConnection(aDispatcher.sink<PacketEvent<TradeRequest>>().connect<&TradeService::OnTradeRequest>(this))
{
}

void TradeService::OnUpdate(const UpdateEvent&) noexcept
{
    if (m_invitations.empty())
        return;

    const auto cTick = GameServer::Get()->GetTick();
    for (auto itor = m_invitations.begin(); itor != m_invitations.end();)
    {
        if (itor->second.ExpiryTick < cTick)
        {
            Send(itor->first, TradeEvent::Closed, itor->second.InviteeId, TradeCloseReason::Expired);
            Send(itor->second.InviteeId, TradeEvent::Closed, itor->first, TradeCloseReason::Expired);
            itor = m_invitations.erase(itor);
        }
        else
            ++itor;
    }
}

void TradeService::OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept
{
    const uint32_t cId = acEvent.pPlayer->GetId();

    for (auto itor = m_invitations.begin(); itor != m_invitations.end();)
    {
        if (itor->first == cId || itor->second.InviteeId == cId)
        {
            const uint32_t cOther = itor->first == cId ? itor->second.InviteeId : itor->first;
            Send(cOther, TradeEvent::Closed, cId, TradeCloseReason::Disconnected);
            itor = m_invitations.erase(itor);
        }
        else
            ++itor;
    }

    size_t side = 0;
    size_t index = 0;
    Trade* pTrade = Find(cId, &side, &index);
    if (!pTrade)
        return;

    pTrade->Sides[side].PlayerId = 0;

    if (pTrade->HandingOver)
    {
        // Whatever the leaving player already gave is gone with them; the other side gets theirs back.
        pTrade->Sides[side].State = HandOver::Failed;
        FinishHandOver(index);
        return;
    }

    CloseTrade(index, TradeCloseReason::Disconnected);
}

void TradeService::OnTradeRequest(const PacketEvent<TradeRequest>& acPacket) noexcept
{
    const auto& message = acPacket.Packet;
    Player* const pPlayer = acPacket.pPlayer;
    const uint32_t cId = pPlayer->GetId();

    switch (message.Action)
    {
    case TradeAction::Invite: Invite(pPlayer, message.PlayerId); break;
    case TradeAction::Accept: Accept(pPlayer, message.PlayerId); break;
    case TradeAction::Decline: Decline(pPlayer, message.PlayerId); break;
    case TradeAction::Cancel: Cancel(cId); break;
    case TradeAction::UpdateOffer: UpdateOffer(cId, message.Offer); break;
    case TradeAction::SetReady: SetReady(cId, message.Ready); break;
    case TradeAction::OfferRemoved: ReportHandOver(cId, true); break;
    case TradeAction::RemoveFailed: ReportHandOver(cId, false); break;
    default: spdlog::warn("[TradeService]: unknown action {} from {}", static_cast<int>(message.Action), cId); break;
    }
}

void TradeService::Invite(Player* apInviter, uint32_t aInviteeId) noexcept
{
    const uint32_t cInviterId = apInviter->GetId();
    Player* const pInvitee = m_world.GetPlayerManager().GetById(aInviteeId);
    if (!pInvitee || pInvitee == apInviter || Find(cInviterId))
        return;

    if (Find(aInviteeId))
    {
        Send(cInviterId, TradeEvent::Closed, aInviteeId, TradeCloseReason::Busy);
        return;
    }

    // One outstanding invitation per player; a new one replaces the old. Asking the same player again while they
    // haven't answered doesn't pile up notifications on their screen.
    if (auto itor = m_invitations.find(cInviterId); itor != m_invitations.end())
    {
        if (itor->second.InviteeId == aInviteeId)
            return;
        Send(itor->second.InviteeId, TradeEvent::Closed, cInviterId, TradeCloseReason::Cancelled);
    }

    m_invitations[cInviterId] = {aInviteeId, GameServer::Get()->GetTick() + kInvitationLifetimeMs};

    spdlog::debug("[TradeService]: {} invites {} to trade", cInviterId, aInviteeId);
    Send(aInviteeId, TradeEvent::Invited, cInviterId);
}

void TradeService::Accept(Player* apInvitee, uint32_t aInviterId) noexcept
{
    const uint32_t cInviteeId = apInvitee->GetId();
    auto itor = m_invitations.find(aInviterId);
    if (itor == m_invitations.end() || itor->second.InviteeId != cInviteeId)
        return;
    m_invitations.erase(itor);

    if (!m_world.GetPlayerManager().GetById(aInviterId))
        return;

    if (Find(aInviterId) || Find(cInviteeId))
    {
        Send(cInviteeId, TradeEvent::Closed, aInviterId, TradeCloseReason::Busy);
        Send(aInviterId, TradeEvent::Closed, cInviteeId, TradeCloseReason::Busy);
        return;
    }

    // Accepting one trade withdraws the invitee's own invitation, if any.
    if (auto own = m_invitations.find(cInviteeId); own != m_invitations.end())
    {
        Send(own->second.InviteeId, TradeEvent::Closed, cInviteeId, TradeCloseReason::Cancelled);
        m_invitations.erase(own);
    }

    Trade trade{};
    trade.Sides[0].PlayerId = aInviterId;
    trade.Sides[1].PlayerId = cInviteeId;
    m_trades.push_back(std::move(trade));

    spdlog::info("[TradeService]: trade opened between {} and {}", aInviterId, cInviteeId);
    Send(aInviterId, TradeEvent::Opened, cInviteeId);
    Send(cInviteeId, TradeEvent::Opened, aInviterId);
}

void TradeService::Decline(Player* apInvitee, uint32_t aInviterId) noexcept
{
    auto itor = m_invitations.find(aInviterId);
    if (itor == m_invitations.end() || itor->second.InviteeId != apInvitee->GetId())
        return;
    m_invitations.erase(itor);
    Send(aInviterId, TradeEvent::Closed, apInvitee->GetId(), TradeCloseReason::Declined);
}

void TradeService::Cancel(uint32_t aPlayerId) noexcept
{
    if (auto itor = m_invitations.find(aPlayerId); itor != m_invitations.end())
    {
        Send(itor->second.InviteeId, TradeEvent::Closed, aPlayerId, TradeCloseReason::Cancelled);
        m_invitations.erase(itor);
    }

    size_t index = 0;
    Trade* pTrade = Find(aPlayerId, nullptr, &index);
    // Once the hand-over started it has to finish, so a cancel then is ignored.
    if (pTrade && !pTrade->HandingOver)
        CloseTrade(index, TradeCloseReason::Cancelled);
}

void TradeService::UpdateOffer(uint32_t aPlayerId, const Inventory& acOffer) noexcept
{
    size_t side = 0;
    Trade* pTrade = Find(aPlayerId, &side);
    if (!pTrade || pTrade->HandingOver)
        return;

    Inventory offer{};
    for (const auto& entry : acOffer.Entries)
    {
        if (entry.Count <= 0 || entry.IsQuestItem || offer.Entries.size() >= kMaxOfferEntries)
            continue;
        offer.Entries.push_back(entry);
    }

    auto& self = pTrade->Sides[side];
    auto& partner = pTrade->Sides[1 - side];
    self.Offer = std::move(offer);

    // Any change to either offer means both players have to look again.
    self.Ready = false;
    partner.Ready = false;

    Send(partner.PlayerId, TradeEvent::PartnerOffer, self.PlayerId, TradeCloseReason::Completed, &self.Offer);
    SendState(*pTrade);
}

void TradeService::SetReady(uint32_t aPlayerId, bool aReady) noexcept
{
    size_t side = 0;
    size_t index = 0;
    Trade* pTrade = Find(aPlayerId, &side, &index);
    if (!pTrade || pTrade->HandingOver)
        return;

    // Nothing on either side: there is nothing to accept.
    if (aReady && pTrade->Sides[0].Offer.Entries.empty() && pTrade->Sides[1].Offer.Entries.empty())
        aReady = false;

    pTrade->Sides[side].Ready = aReady;
    SendState(*pTrade);

    if (!pTrade->Sides[0].Ready || !pTrade->Sides[1].Ready)
        return;

    // Both agreed. Step 1: each player takes their own offer out of their inventory.
    pTrade->HandingOver = true;
    for (auto& s : pTrade->Sides)
    {
        if (s.Offer.Entries.empty())
            s.State = HandOver::Removed;
        else
            Send(s.PlayerId, TradeEvent::RemoveOffer, 0, TradeCloseReason::Completed, &s.Offer);
    }

    FinishHandOver(index);
}

void TradeService::ReportHandOver(uint32_t aPlayerId, bool aRemoved) noexcept
{
    size_t side = 0;
    size_t index = 0;
    Trade* pTrade = Find(aPlayerId, &side, &index);
    if (!pTrade || !pTrade->HandingOver || pTrade->Sides[side].State != HandOver::Pending)
        return;

    pTrade->Sides[side].State = aRemoved ? HandOver::Removed : HandOver::Failed;
    FinishHandOver(index);
}

void TradeService::FinishHandOver(size_t aTradeIndex) noexcept
{
    const Trade& trade = m_trades[aTradeIndex];
    if (trade.Sides[0].State == HandOver::Pending || trade.Sides[1].State == HandOver::Pending)
        return;

    const bool cSuccess = trade.Sides[0].State == HandOver::Removed && trade.Sides[1].State == HandOver::Removed;
    for (size_t i = 0; i < 2; ++i)
    {
        const Side& self = trade.Sides[i];
        const Side& partner = trade.Sides[1 - i];

        // Step 2: receive the partner's offer, or, if the trade fell through, get back what you gave.
        if (cSuccess && !partner.Offer.Entries.empty())
            Send(self.PlayerId, TradeEvent::Receive, partner.PlayerId, TradeCloseReason::Completed, &partner.Offer);
        else if (!cSuccess && self.State == HandOver::Removed && !self.Offer.Entries.empty())
            Send(self.PlayerId, TradeEvent::Receive, partner.PlayerId, TradeCloseReason::Failed, &self.Offer);
    }

    spdlog::info("[TradeService]: trade between {} and {} {}", trade.Sides[0].PlayerId, trade.Sides[1].PlayerId, cSuccess ? "completed" : "failed, offers returned");
    CloseTrade(aTradeIndex, cSuccess ? TradeCloseReason::Completed : TradeCloseReason::Failed);
}

TradeService::Trade* TradeService::Find(uint32_t aPlayerId, size_t* apSide, size_t* apIndex) noexcept
{
    if (!aPlayerId)
        return nullptr;

    for (size_t i = 0; i < m_trades.size(); ++i)
    {
        for (size_t s = 0; s < 2; ++s)
        {
            if (m_trades[i].Sides[s].PlayerId == aPlayerId)
            {
                if (apSide)
                    *apSide = s;
                if (apIndex)
                    *apIndex = i;
                return &m_trades[i];
            }
        }
    }
    return nullptr;
}

void TradeService::Send(uint32_t aPlayerId, TradeEvent aEvent, uint32_t aPartnerId, TradeCloseReason aReason, const Inventory* apItems) const noexcept
{
    if (!aPlayerId)
        return;

    Player* const pPlayer = m_world.GetPlayerManager().GetById(aPlayerId);
    if (!pPlayer)
        return;

    NotifyTrade notify;
    notify.Event = aEvent;
    notify.PartnerId = aPartnerId;
    notify.Reason = aReason;
    if (apItems)
        notify.Items = *apItems;
    pPlayer->Send(notify);
}

void TradeService::SendState(const Trade& acTrade) const noexcept
{
    for (size_t i = 0; i < 2; ++i)
    {
        Player* const pPlayer = acTrade.Sides[i].PlayerId ? m_world.GetPlayerManager().GetById(acTrade.Sides[i].PlayerId) : nullptr;
        if (!pPlayer)
            continue;

        NotifyTrade notify;
        notify.Event = TradeEvent::State;
        notify.PartnerId = acTrade.Sides[1 - i].PlayerId;
        notify.SelfReady = acTrade.Sides[i].Ready;
        notify.PartnerReady = acTrade.Sides[1 - i].Ready;
        pPlayer->Send(notify);
    }
}

void TradeService::CloseTrade(size_t aTradeIndex, TradeCloseReason aReason) noexcept
{
    const uint32_t cFirst = m_trades[aTradeIndex].Sides[0].PlayerId;
    const uint32_t cSecond = m_trades[aTradeIndex].Sides[1].PlayerId;
    m_trades.erase(m_trades.begin() + aTradeIndex);

    Send(cFirst, TradeEvent::Closed, cSecond, aReason);
    Send(cSecond, TradeEvent::Closed, cFirst, aReason);
}
