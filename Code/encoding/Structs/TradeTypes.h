#pragma once

// Player-to-player trading. The server keeps each trade's two offers and both players' "ready" flags; items only
// change hands once both players are ready. Hand-over happens in two steps so nothing can be duplicated: each client
// first takes its own offer out of its inventory and reports back, and only when both have done so does the server
// tell each client to add what the other offered. If either side fails, the side that already gave its items gets
// them back.
enum class TradeAction : uint8_t
{
    Invite,       // ask PlayerId to trade
    Accept,       // accept PlayerId's invitation
    Decline,      // decline PlayerId's invitation
    Cancel,       // close the open trade (or withdraw an invitation)
    UpdateOffer,  // Offer is the full new offer
    SetReady,     // Ready
    OfferRemoved, // hand-over step 1 done: the offer left this player's inventory
    RemoveFailed  // hand-over step 1 failed: this player no longer has everything they offered
};

enum class TradeEvent : uint8_t
{
    Invited,      // PartnerId wants to trade
    Opened,       // the trade with PartnerId is open
    Closed,       // the trade or invitation ended, see Reason
    PartnerOffer, // Items is the partner's current offer
    State,        // SelfReady / PartnerReady changed
    RemoveOffer,  // hand-over step 1: take Items (your own offer) out of your inventory, then report back
    Receive       // hand-over step 2: add Items to your inventory (the partner's offer, or your own refunded)
};

enum class TradeCloseReason : uint8_t
{
    Completed,
    Cancelled,
    Declined,
    Busy, // the other player is already trading
    Disconnected,
    Failed, // the hand-over failed; anything you gave was returned
    Expired // the invitation wasn't answered in time
};
