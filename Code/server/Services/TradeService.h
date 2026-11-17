#pragma once

#include <Events/PacketEvent.h>
#include <Structs/Inventory.h>
#include <Structs/TradeTypes.h>

struct World;
struct UpdateEvent;
struct PlayerLeaveEvent;
struct TradeRequest;

/**
 * @brief Player-to-player trades: invitations, both offers, and a hand-over that can't duplicate items.
 *
 * See TradeTypes.h for the protocol. The server keeps the agreed offers itself, so what each client gives and receives
 * is exactly what both players saw when they pressed "Accept".
 */
struct TradeService
{
    TradeService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~TradeService() noexcept = default;

    TP_NOCOPYMOVE(TradeService);

private:
    enum class HandOver : uint8_t
    {
        Pending,
        Removed,
        Failed
    };

    struct Side
    {
        uint32_t PlayerId{};
        Inventory Offer{};
        bool Ready{};
        HandOver State{HandOver::Pending};
    };

    struct Trade
    {
        Side Sides[2]{};
        bool HandingOver{};
    };

    struct Invitation
    {
        uint32_t InviteeId{};
        uint64_t ExpiryTick{};
    };

    void OnUpdate(const UpdateEvent&) noexcept;
    void OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept;
    void OnTradeRequest(const PacketEvent<TradeRequest>& acPacket) noexcept;

    void Invite(Player* apInviter, uint32_t aInviteeId) noexcept;
    void Accept(Player* apInvitee, uint32_t aInviterId) noexcept;
    void Decline(Player* apInvitee, uint32_t aInviterId) noexcept;
    void Cancel(uint32_t aPlayerId) noexcept;
    void UpdateOffer(uint32_t aPlayerId, const Inventory& acOffer) noexcept;
    void SetReady(uint32_t aPlayerId, bool aReady) noexcept;
    void ReportHandOver(uint32_t aPlayerId, bool aRemoved) noexcept;
    void FinishHandOver(size_t aTradeIndex) noexcept;

    // The trade the player is in (and which side they are, and its index), or nullptr.
    Trade* Find(uint32_t aPlayerId, size_t* apSide = nullptr, size_t* apIndex = nullptr) noexcept;

    void
    Send(uint32_t aPlayerId, TradeEvent aEvent, uint32_t aPartnerId, TradeCloseReason aReason = TradeCloseReason::Completed, const Inventory* apItems = nullptr) const noexcept;
    void SendState(const Trade& acTrade) const noexcept;
    void CloseTrade(size_t aTradeIndex, TradeCloseReason aReason) noexcept;

    World& m_world;
    Vector<Trade> m_trades;
    Map<uint32_t, Invitation> m_invitations; // inviter id -> invitation

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_playerLeaveConnection;
    entt::scoped_connection m_tradeRequestConnection;
};
