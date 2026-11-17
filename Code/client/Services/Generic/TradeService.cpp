#include <TiltedOnlinePCH.h>

#include <Services/TradeService.h>

#include <Services/OverlayService.h>
#include <Services/TransportService.h>
#include <World.h>

#include <Events/DisconnectedEvent.h>
#include <Messages/NotifyTrade.h>
#include <Messages/TradeRequest.h>

#include <Components/TESValueForm.h>
#include <ExtraData/ExtraDataList.h>
#include <Forms/AlchemyItem.h>
#include <Games/References.h>
#include <PlayerCharacter.h>

#include <DInputHook.hpp>
#include <OverlayApp.hpp>

namespace
{
// Form types that are inventory items a player can hand over.
bool IsTradableType(FormType aType) noexcept
{
    switch (static_cast<uint8_t>(aType))
    {
    case 23: // scroll
    case 26: // armor
    case 27: // book
    case 30: // ingredient
    case 31: // light (torch)
    case 32: // misc item, including gold
    case 33: // apparatus
    case 41: // weapon
    case 42: // ammo
    case 45: // key
    case 46: // potion / poison / food
    case 52: // soul gem
        return true;
    default: return false;
    }
}

const char* CategoryOf(const TESForm* apForm) noexcept
{
    switch (static_cast<uint8_t>(apForm->formType))
    {
    case 41: return "weapons";
    case 42: return "weapons";
    case 26: return "apparel";
    case 46: return "potions";
    case 30: return "ingredients";
    case 23: return "scrolls";
    case 27: return "books";
    case 45: return "keys";
    default: return apForm->formID == 0xF ? "gold" : "misc";
    }
}

void AppendJsonString(String& aOut, const char* acText) noexcept
{
    aOut += '"';
    for (const char* p = acText ? acText : ""; *p; ++p)
    {
        const char c = *p;
        if (c == '"' || c == '\\')
        {
            aOut += '\\';
            aOut += c;
        }
        else if (static_cast<unsigned char>(c) < 0x20)
            aOut += ' ';
        else
            aOut += c;
    }
    aOut += '"';
}

// The form an entry stands for on this client. Player-made potions are rebuilt from their effects; aCreated holds
// the temporary engine reference while the form is used.
TESForm* ResolveForm(const Inventory::Entry& acEntry, std::optional<CreatedAlchemyItemRef>& aCreated) noexcept
{
    if (!acEntry.CustomAlchemy.IsEmpty())
    {
        aCreated.emplace(AlchemyItem::Create(acEntry.CustomAlchemy));
        return aCreated->pItem;
    }
    return TESForm::GetById(World::Get().GetModSystem().GetGameId(acEntry.BaseId));
}

// JSON array describing items for the trade menu; "i" is the item's index in acEntries.
String DescribeItems(const Vector<Inventory::Entry>& acEntries) noexcept
{
    String json = "[";
    for (size_t i = 0; i < acEntries.size(); ++i)
    {
        const auto& entry = acEntries[i];
        std::optional<CreatedAlchemyItemRef> created;
        TESForm* pForm = ResolveForm(entry, created);

        int32_t value = -1;
        float weight = 0.f;
        if (pForm)
        {
            if (auto* pValue = Cast<TESValueForm>(pForm))
                value = pValue->value;
            else if (auto* pAlchemy = Cast<AlchemyItem>(pForm); pAlchemy && (pAlchemy->flags & 1)) // manual cost
                value = pAlchemy->costOverride;
            if (auto* pWeight = Cast<TESWeightForm>(pForm))
                weight = pWeight->weight;
        }

        if (i)
            json += ',';
        json += "{\"i\":" + std::to_string(i) + ",\"n\":";
        AppendJsonString(json, pForm ? pForm->GetName() : "Unknown item");
        json += ",\"c\":" + std::to_string(entry.Count);
        json += ",\"v\":" + std::to_string(value);
        json += ",\"w\":" + std::to_string(weight);
        json += ",\"t\":";
        AppendJsonString(json, pForm ? CategoryOf(pForm) : "misc");
        const bool cEnchanted = static_cast<bool>(entry.ExtraEnchantId) || !entry.EnchantData.Effects.empty();
        json += std::string(",\"e\":") + (cEnchanted ? "true" : "false");
        json += std::string(",\"p\":") + (!entry.ExtraPoisonData.IsEmpty() || static_cast<bool>(entry.ExtraPoisonId) ? "true" : "false");
        json += "}";
    }
    json += "]";
    return json;
}

void Execute(const char* acpEvent, CefRefPtr<CefListValue> apArguments = nullptr) noexcept
{
    if (auto* pApp = World::Get().GetOverlayService().GetOverlayApp())
        pApp->ExecuteAsync(acpEvent, apArguments);
}

// How many of exactly this item (same extra data) the player has, across every stack.
int32_t CountMatching(const PlayerCharacter& acPlayer, const Inventory::Entry& acWanted) noexcept
{
    int32_t count = 0;
    for (const auto& owned : acPlayer.GetInventory().Entries)
    {
        if (owned.CanBeMerged(acWanted))
            count += owned.Count;
    }
    return count;
}

// Opens the overlay with the mouse cursor, the same way F2 does, so the trade window shows up on both screens
// (the player who was invited may have accepted from the notification, or not be looking at the overlay at all).
void ShowOverlay() noexcept
{
    auto& overlay = World::Get().GetOverlayService();
    auto* pApp = overlay.GetOverlayApp();
    if (!pApp || !overlay.GetInGame() || overlay.GetActive())
        return;
    auto pClient = pApp->GetClient();
    if (!pClient)
        return;
    auto pRenderer = pClient->GetOverlayRenderHandler();
    if (!pRenderer)
        return;

    TiltedPhoques::DInputHook::Get().SetEnabled(true);
    overlay.SetActive(true);
    pRenderer->SetCursorVisible(true);
    while (ShowCursor(FALSE) >= 0) // keep the Windows cursor hidden, the overlay draws its own
        ;
}

bool SameItem(const Inventory::Entry& acLhs, const Inventory::Entry& acRhs) noexcept
{
    return acLhs.BaseId == acRhs.BaseId && acLhs.CustomAlchemy == acRhs.CustomAlchemy;
}
} // namespace

TradeService::TradeService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_transport(aTransport)
{
    m_notifyConnection = aDispatcher.sink<NotifyTrade>().connect<&TradeService::OnNotifyTrade>(this);
    m_disconnectedConnection = aDispatcher.sink<DisconnectedEvent>().connect<&TradeService::OnDisconnected>(this);
}

void TradeService::Invite(uint32_t aPlayerId) noexcept
{
    TradeRequest request;
    request.Action = TradeAction::Invite;
    request.PlayerId = aPlayerId;
    m_transport.Send(request);
}

void TradeService::Accept(uint32_t aInviterId) noexcept
{
    TradeRequest request;
    request.Action = TradeAction::Accept;
    request.PlayerId = aInviterId;
    m_transport.Send(request);
}

void TradeService::Decline(uint32_t aInviterId) noexcept
{
    TradeRequest request;
    request.Action = TradeAction::Decline;
    request.PlayerId = aInviterId;
    m_transport.Send(request);
}

void TradeService::Cancel() noexcept
{
    TradeRequest request;
    request.Action = TradeAction::Cancel;
    m_transport.Send(request);
}

void TradeService::SetOfferCount(uint32_t aItemIndex, int32_t aCount) noexcept
{
    if (!m_partnerId || m_handingOver || aItemIndex >= m_tradable.size())
        return;

    m_offerCounts[aItemIndex] = std::clamp(aCount, 0, m_tradable[aItemIndex].Count);
    SendOffer();
}

void TradeService::SetReady(bool aReady) noexcept
{
    if (!m_partnerId || m_handingOver)
        return;

    TradeRequest request;
    request.Action = TradeAction::SetReady;
    request.Ready = aReady;
    m_transport.Send(request);
}

void TradeService::OnNotifyTrade(const NotifyTrade& acMessage) noexcept
{
    switch (acMessage.Event)
    {
    case TradeEvent::Invited:
    {
        spdlog::info("[TradeService]: player {} wants to trade", acMessage.PartnerId);
        auto pArguments = CefListValue::Create();
        pArguments->SetInt(0, acMessage.PartnerId);
        Execute("tradeInvited", pArguments);
        break;
    }
    case TradeEvent::Opened:
    {
        spdlog::info("[TradeService]: trade with player {} opened", acMessage.PartnerId);
        m_partnerId = acMessage.PartnerId;
        m_handingOver = false;
        RefreshTradableItems();

        auto pArguments = CefListValue::Create();
        pArguments->SetInt(0, m_partnerId);
        pArguments->SetString(1, DescribeItems(m_tradable).c_str());
        Execute("tradeOpened", pArguments);
        ShowOverlay();
        break;
    }
    case TradeEvent::PartnerOffer:
    {
        auto pArguments = CefListValue::Create();
        pArguments->SetString(0, DescribeItems(acMessage.Items.Entries).c_str());
        Execute("tradePartnerOffer", pArguments);
        break;
    }
    case TradeEvent::State:
    {
        auto pArguments = CefListValue::Create();
        pArguments->SetBool(0, acMessage.SelfReady);
        pArguments->SetBool(1, acMessage.PartnerReady);
        Execute("tradeState", pArguments);
        break;
    }
    case TradeEvent::RemoveOffer:
    {
        m_handingOver = true;
        Execute("tradeHandingOver");

        TradeRequest request;
        request.Action = RemoveOffer(acMessage.Items) ? TradeAction::OfferRemoved : TradeAction::RemoveFailed;
        m_transport.Send(request);
        break;
    }
    case TradeEvent::Receive:
        spdlog::info("[TradeService]: receiving {} item stacks{}", acMessage.Items.Entries.size(), acMessage.Reason == TradeCloseReason::Failed ? " (returned)" : "");
        Receive(acMessage.Items);
        break;
    case TradeEvent::Closed:
    {
        spdlog::info("[TradeService]: trade with player {} closed, reason {}", acMessage.PartnerId, static_cast<int>(acMessage.Reason));
        Reset();
        auto pArguments = CefListValue::Create();
        pArguments->SetInt(0, static_cast<int>(acMessage.Reason));
        pArguments->SetInt(1, acMessage.PartnerId);
        Execute("tradeClosed", pArguments);
        break;
    }
    default: break;
    }
}

void TradeService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    if (!m_partnerId)
        return;

    Reset();
    auto pArguments = CefListValue::Create();
    pArguments->SetInt(0, static_cast<int>(TradeCloseReason::Disconnected));
    pArguments->SetInt(1, 0);
    Execute("tradeClosed", pArguments);
}

void TradeService::RefreshTradableItems() noexcept
{
    m_tradable.clear();
    m_offerCounts.clear();

    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pPlayer)
        return;

    // Equipped items and quest items stay with the player.
    Inventory inventory = pPlayer->GetInventory([](TESForm& aForm) { return IsTradableType(aForm.formType); });
    for (auto& entry : inventory.Entries)
    {
        if (entry.Count <= 0 || entry.IsWorn() || entry.IsQuestItem)
            continue;
        m_tradable.push_back(entry);
    }

    std::sort(
        m_tradable.begin(), m_tradable.end(),
        [](const Inventory::Entry& acLhs, const Inventory::Entry& acRhs)
        {
            // gold first, then by form id for a stable order (the menu sorts by name)
            if ((acLhs.BaseId.BaseId == 0xF) != (acRhs.BaseId.BaseId == 0xF))
                return acLhs.BaseId.BaseId == 0xF;
            return acLhs.BaseId.BaseId < acRhs.BaseId.BaseId;
        });
    m_offerCounts.resize(m_tradable.size(), 0);
}

void TradeService::SendOffer() noexcept
{
    TradeRequest request;
    request.Action = TradeAction::UpdateOffer;
    for (size_t i = 0; i < m_tradable.size(); ++i)
    {
        if (m_offerCounts[i] <= 0)
            continue;
        Inventory::Entry entry = m_tradable[i];
        entry.Count = m_offerCounts[i];
        request.Offer.Entries.push_back(std::move(entry));
    }
    m_transport.Send(request);

    // The menu shows the offer as this client holds it: {"<item index>": count, ...}
    String json = "{";
    for (size_t i = 0; i < m_offerCounts.size(); ++i)
    {
        if (m_offerCounts[i] <= 0)
            continue;
        if (json.size() > 1)
            json += ',';
        json += "\"" + std::to_string(i) + "\":" + std::to_string(m_offerCounts[i]);
    }
    json += "}";
    auto pArguments = CefListValue::Create();
    pArguments->SetString(0, json.c_str());
    Execute("tradeMyOffer", pArguments);
}

// Hand-over step 1: take the agreed offer out of the player's inventory. Checks everything is still there first
// (the player may have dropped, equipped or used something since), so either all of it goes or none of it.
bool TradeService::RemoveOffer(const Inventory& acOffer) noexcept
{
    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pPlayer)
        return false;

    for (const auto& wanted : acOffer.Entries)
    {
        const int32_t available = CountMatching(*pPlayer, wanted);
        if (available < wanted.Count)
        {
            spdlog::warn("[TradeService]: offered {:X}:{:X} x{} but only {} left", wanted.BaseId.ModId, wanted.BaseId.BaseId, wanted.Count, available);
            return false;
        }
    }

    Inventory removed{};
    for (const auto& wanted : acOffer.Entries)
    {
        Inventory::Entry taken = wanted;
        taken.Count = RemoveItems(wanted);
        if (taken.Count > 0)
            removed.Entries.push_back(taken);

        if (taken.Count != wanted.Count)
        {
            // Shouldn't happen after the check above; put back what already left so nothing is lost.
            spdlog::error("[TradeService]: could only hand over {} of {:X}:{:X} x{}, trade cancelled", taken.Count, wanted.BaseId.ModId, wanted.BaseId.BaseId, wanted.Count);
            Receive(removed);
            return false;
        }
    }

    return true;
}

// Removes up to acWanted.Count of exactly that item (same extra data: enchantment, tempering, poison...) from the
// player, and returns how many were removed. Equipped copies are never taken.
int32_t TradeService::RemoveItems(const Inventory::Entry& acWanted) noexcept
{
    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    int32_t remaining = acWanted.Count;

    // Removing can free the list nodes being walked, so look the stack up again after every removal.
    for (int attempt = 0; remaining > 0 && attempt < 64; ++attempt)
    {
        TESBoundObject* pObject = nullptr;
        ExtraDataList* pStack = nullptr;
        int32_t stackCount = remaining;

        auto* pChanges = pPlayer->GetContainerChanges();
        if (pChanges && pChanges->entries)
        {
            for (auto* pGameEntry : *pChanges->entries)
            {
                if (!pGameEntry || !pGameEntry->form)
                    continue;

                Inventory::Entry item{};
                TESObjectREFR::SetItemBase(item, pGameEntry->form);
                if (!SameItem(item, acWanted))
                    continue;

                pObject = Cast<TESBoundObject>(pGameEntry->form);
                if (!pGameEntry->dataList)
                    break;

                for (ExtraDataList* pExtra : *pGameEntry->dataList)
                {
                    if (!pExtra)
                        continue;
                    Inventory::Entry stack{};
                    stack.Count = 1;
                    TESObjectREFR::GetItemFromExtraData(stack, pExtra);
                    // A plain item can sit in a stack that only carries a count; an item with extra data always has
                    // its own stack.
                    if (stack.IsExtraDataEquals(acWanted))
                    {
                        pStack = pExtra;
                        stackCount = stack.Count;
                        break;
                    }
                }
                break;
            }
        }

        // Items still only in the base container (never touched) have no changes entry yet.
        if (!pObject && !acWanted.ContainsExtraData())
        {
            std::optional<CreatedAlchemyItemRef> created;
            pObject = Cast<TESBoundObject>(ResolveForm(acWanted, created));
        }

        if (!pObject || (acWanted.ContainsExtraData() && !pStack))
            break;

        // Measure what actually left through the full inventory (base container + every stack); the engine's
        // per-form count misses items the save already had, which made this remove too much.
        const int32_t cCount = std::min(remaining, std::max(stackCount, 1));
        const int32_t cBefore = CountMatching(*pPlayer, acWanted);
        pPlayer->RemoveItem(pObject, cCount, ITEM_REMOVE_REASON::kRemove, pStack, nullptr);
        const int32_t cGone = cBefore - CountMatching(*pPlayer, acWanted);
        if (cGone <= 0)
            break;
        remaining -= cGone;
    }

    return acWanted.Count - remaining;
}

// Hand-over step 2: add the partner's offer (or our own, returned).
void TradeService::Receive(const Inventory& acItems) noexcept
{
    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pPlayer)
        return;

    for (auto entry : acItems.Entries)
    {
        entry.ExtraWorn = false;
        entry.ExtraWornLeft = false;
        entry.IsQuestItem = false;
        if (entry.Count > 0)
            pPlayer->AddOrRemoveItem(entry);
    }
}

void TradeService::Reset() noexcept
{
    m_partnerId = 0;
    m_handingOver = false;
    m_tradable.clear();
    m_offerCounts.clear();
}
