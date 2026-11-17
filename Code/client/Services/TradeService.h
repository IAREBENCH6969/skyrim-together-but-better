#pragma once

#include <Structs/Inventory.h>

struct World;
struct TransportService;
struct DisconnectedEvent;
struct NotifyTrade;

/**
 * @brief The local player's side of a player-to-player trade.
 *
 * Builds the list of items the player can trade, keeps their offer, talks to the server's TradeService, and drives
 * the trade menu in the overlay. Items change hands in two steps (see TradeTypes.h): this client first takes its own
 * offer out of the inventory, then adds what the partner offered.
 */
struct TradeService
{
    TradeService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~TradeService() noexcept = default;

    TP_NOCOPYMOVE(TradeService);

    // Called from the overlay, on the game thread.
    void Invite(uint32_t aPlayerId) noexcept;
    void Accept(uint32_t aInviterId) noexcept;
    void Decline(uint32_t aInviterId) noexcept;
    void Cancel() noexcept;
    // Sets how many of the tradable item aItemIndex (an index into the list sent to the menu) are offered; 0 removes it.
    void SetOfferCount(uint32_t aItemIndex, int32_t aCount) noexcept;
    void SetReady(bool aReady) noexcept;

    [[nodiscard]] bool IsTrading() const noexcept { return m_partnerId != 0; }
    [[nodiscard]] uint32_t GetPartnerId() const noexcept { return m_partnerId; }
    // The items the menu lists, in the order SetOfferCount indexes them.
    [[nodiscard]] const Vector<Inventory::Entry>& GetTradableItems() const noexcept { return m_tradable; }

private:
    void OnNotifyTrade(const NotifyTrade& acMessage) noexcept;
    void OnDisconnected(const DisconnectedEvent&) noexcept;

    void RefreshTradableItems() noexcept;
    void SendOffer() noexcept;
    bool RemoveOffer(const Inventory& acOffer) noexcept;
    int32_t RemoveItems(const Inventory::Entry& acWanted) noexcept;
    void Receive(const Inventory& acItems) noexcept;
    void Reset() noexcept;

    World& m_world;
    TransportService& m_transport;

    uint32_t m_partnerId{};
    bool m_handingOver{};
    bool m_openedOverlay{};
    Vector<Inventory::Entry> m_tradable;
    Vector<int32_t> m_offerCounts; // parallel to m_tradable

    entt::scoped_connection m_notifyConnection;
    entt::scoped_connection m_disconnectedConnection;
};
