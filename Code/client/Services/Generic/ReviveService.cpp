#include <TiltedOnlinePCH.h>

#include <Services/ReviveService.h>

#include <Services/PartyService.h>
#include <Services/PlayerService.h>
#include <Services/TransportService.h>
#include <World.h>

#include <Components.h>
#include <Events/DisconnectedEvent.h>
#include <Events/UpdateEvent.h>
#include <Messages/NotifyRevive.h>
#include <Messages/ReviveRequest.h>
#include <Structs/ServerSettings.h>

#include <ExtraData/ExtraContainerChanges.h>
#include <Forms/EffectSetting.h>
#include <Forms/MagicItem.h>
#include <Magic/EffectItem.h>
#include <PlayerCharacter.h>
#include <Interface/UI.h>
#include <Services/OverlayService.h>

namespace
{
constexpr uint32_t kRestoreHealth = 0x3EB15;
// A teammate this close (game units, about 6 m) to a downed player gets a reminder of how to revive them.
constexpr float kHintDistance = 400.f;
// Reach for a revive (about 3 m): the downed player has to be this close and roughly in front of the reviver.
constexpr float kReviveDistance = 200.f;

// ALCH data flags (ENIT), right after the cost: food and poison items aren't healing potions.
uint32_t AlchemyFlags(const TESForm* apForm) noexcept
{
    return *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(apForm) + 0x13C);
}
constexpr uint32_t kAlchemyFood = 0x2;
constexpr uint32_t kAlchemyPoison = 0x20000;

// The Activate key (E). The game never offers a bleeding-out character as an activation target, so the revive
// watches the key itself, while the player is in the game (no menu, console or overlay open).
constexpr int kActivateKey = 'E';

bool PlayerIsInGame() noexcept
{
    DWORD processId = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &processId);
    if (processId != GetCurrentProcessId())
        return false;
    if (World::Get().GetOverlayService().GetActive())
        return false;
    UI* pUI = UI::Get();
    return pUI && pUI->numPausesGame == 0 && !pUI->modal && !pUI->GetMenuOpen("Console") && !pUI->IsUnpausedMenuOpen();
}

const char* FormName(const TESForm* apForm) noexcept
{
    const char* cpName = apForm ? reinterpret_cast<const MagicItem*>(apForm)->fullName.value.AsAscii() : nullptr;
    return cpName && *cpName ? cpName : "a healing potion";
}
} // namespace

ReviveService::ReviveService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_transport(aTransport)
{
    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&ReviveService::OnUpdate>(this);
    m_disconnectedConnection = aDispatcher.sink<DisconnectedEvent>().connect<&ReviveService::OnDisconnected>(this);
    m_settingsConnection = aDispatcher.sink<ServerSettings>().connect<&ReviveService::OnServerSettings>(this);
    m_notifyConnection = aDispatcher.sink<NotifyRevive>().connect<&ReviveService::OnNotifyRevive>(this);
}

void ReviveService::ReportDowned(bool aDowned) noexcept
{
    if (!m_transport.IsConnected() || m_reviveWindow <= 0.f)
        return;

    ReviveRequest request;
    request.Action = aDowned ? ReviveAction::Downed : ReviveAction::Up;
    m_transport.Send(request);
}

bool ReviveService::TryReviveNearby() noexcept
{
    if (m_downedPlayers.empty())
        return false;

    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    const float cForwardX = std::sin(pPlayer->rotation.z);
    const float cForwardY = std::cos(pPlayer->rotation.z);

    Actor* pBest = nullptr;
    float bestDistance = kReviveDistance;
    auto view = m_world.view<FormIdComponent, PlayerComponent>();
    for (auto entity : view)
    {
        if (!m_downedPlayers.contains(view.get<PlayerComponent>(entity).Id))
            continue;
        auto* pActor = Cast<Actor>(TESForm::GetById(view.get<FormIdComponent>(entity).Id));
        if (!pActor || pActor == pPlayer || pActor->GetParentCellEx() != pPlayer->GetParentCellEx())
            continue;
        const float cDx = pActor->position.x - pPlayer->position.x;
        const float cDy = pActor->position.y - pPlayer->position.y;
        const float cDistance = std::sqrt(cDx * cDx + cDy * cDy);
        // In front: within about 60 degrees of where the player is facing (or practically on top of them).
        if (cDistance > 40.f && (cDx * cForwardX + cDy * cForwardY) < cDistance * 0.5f)
            continue;
        if (cDistance < bestDistance)
        {
            pBest = pActor;
            bestDistance = cDistance;
        }
    }

    return pBest && TryRevive(pBest);
}

bool ReviveService::TryRevive(TESObjectREFR* apTarget) noexcept
{
    if (m_downedPlayers.empty() || !apTarget)
        return false;

    const uint32_t cPlayerId = PlayerIdOf(apTarget);
    if (!cPlayerId || !m_downedPlayers.contains(cPlayerId))
        return false;

    const String cName = NameOf(cPlayerId);
    float health = 0.f;
    TESBoundObject* pPotion = FindHealingPotion(health);
    if (!pPotion)
    {
        Utils::ShowHudMessage(String(fmt::format("You need a healing potion to revive {}", cName)));
        return true;
    }

    const std::string cPotionName = FormName(pPotion);
    PlayerCharacter::Get()->RemoveItem(pPotion, 1, ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

    ReviveRequest request;
    request.Action = ReviveAction::Revive;
    request.PlayerId = cPlayerId;
    request.Health = health;
    m_transport.Send(request);

    m_downedPlayers.erase(cPlayerId);
    spdlog::info("[ReviveService]: revived player {} with {} ({} health)", cPlayerId, cPotionName, health);
    Utils::ShowHudMessage(String(fmt::format("You used {} to revive {}", cPotionName, cName)));
    return true;
}

TESBoundObject* ReviveService::FindHealingPotion(float& aHealth) noexcept
{
    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    auto* pChanges = pPlayer ? pPlayer->GetContainerChanges() : nullptr;
    if (!pChanges || !pChanges->entries)
        return nullptr;

    TESBoundObject* pBest = nullptr;
    float bestHealth = 0.f;
    for (auto* pEntry : *pChanges->entries)
    {
        if (!pEntry || !pEntry->form || pEntry->form->formType != FormType::Alchemy)
            continue;
        if (pPlayer->GetItemCountInInventory(pEntry->form) <= 0)
            continue;
        if (AlchemyFlags(pEntry->form) & (kAlchemyFood | kAlchemyPoison))
            continue;

        const auto& effects = reinterpret_cast<const MagicItem*>(pEntry->form)->listOfEffects;
        for (uint32_t i = 0; i < effects.length; i++)
        {
            const EffectItem* pEffect = effects[i];
            if (!pEffect || !pEffect->pEffectSetting || pEffect->pEffectSetting->formID != kRestoreHealth)
                continue;
            // The weakest one, so a revive doesn't burn the best potion.
            if (!pBest || pEffect->data.fMagnitude < bestHealth)
            {
                pBest = Cast<TESBoundObject>(pEntry->form);
                bestHealth = pEffect->data.fMagnitude;
            }
            break;
        }
    }

    aHealth = bestHealth;
    return pBest;
}

void ReviveService::OnUpdate(const UpdateEvent&) noexcept
{
    if (m_downedPlayers.empty())
        return;

    // A fresh press of Activate next to a downed player revives them.
    const bool cActivateDown = (GetAsyncKeyState(kActivateKey) & 0x8000) != 0;
    if (cActivateDown && !m_activateWasDown && PlayerIsInGame())
        TryReviveNearby();
    m_activateWasDown = cActivateDown;

    const auto cNow = std::chrono::steady_clock::now();
    if (cNow < m_nextUpdate)
        return;
    m_nextUpdate = cNow + 1s;

    // Remind a teammate standing next to a downed player how to get them up.
    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    auto view = m_world.view<FormIdComponent, PlayerComponent>();
    for (auto entity : view)
    {
        const uint32_t cPlayerId = view.get<PlayerComponent>(entity).Id;
        if (!m_downedPlayers.contains(cPlayerId))
            continue;
        auto* pActor = Cast<Actor>(TESForm::GetById(view.get<FormIdComponent>(entity).Id));
        if (!pActor || pActor->GetParentCellEx() != pPlayer->GetParentCellEx())
            continue;
        const float cDx = pActor->position.x - pPlayer->position.x;
        const float cDy = pActor->position.y - pPlayer->position.y;
        if (cDx * cDx + cDy * cDy > kHintDistance * kHintDistance)
            continue;
        if (auto itor = m_nextHint.find(cPlayerId); itor != m_nextHint.end() && cNow < itor->second)
            continue;
        m_nextHint[cPlayerId] = cNow + 8s;

        float health = 0.f;
        TESBoundObject* pPotion = FindHealingPotion(health);
        Utils::ShowHudMessage(String(
            pPotion ? fmt::format("Face {} and press E to revive them with {}", NameOf(cPlayerId), FormName(pPotion))
                    : fmt::format("{} is down. You need a healing potion to revive them", NameOf(cPlayerId))));
    }
}

void ReviveService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    m_downedPlayers.clear();
    m_nextHint.clear();
}

void ReviveService::OnServerSettings(const ServerSettings& acSettings) noexcept
{
    m_reviveWindow = acSettings.DeathSystemEnabled ? acSettings.ReviveWindow : 0.f;
}

void ReviveService::OnNotifyRevive(const NotifyRevive& acMessage) noexcept
{
    switch (acMessage.Event)
    {
    case ReviveEvent::Downed:
        m_downedPlayers.insert(acMessage.PlayerId);
        m_nextHint.erase(acMessage.PlayerId);
        spdlog::info("[ReviveService]: player {} is down", acMessage.PlayerId);
        Utils::ShowHudMessage(String(fmt::format("{} is down! Get to them and press E to use a healing potion", NameOf(acMessage.PlayerId))));
        break;
    case ReviveEvent::Up:
        m_downedPlayers.erase(acMessage.PlayerId);
        spdlog::info("[ReviveService]: player {} is up", acMessage.PlayerId);
        break;
    case ReviveEvent::Revived:
        spdlog::info("[ReviveService]: revived by player {} ({} health)", acMessage.PlayerId, acMessage.Health);
        if (m_world.ctx().at<PlayerService>().Revive(acMessage.Health))
            Utils::ShowHudMessage(String(fmt::format("{} revived you", NameOf(acMessage.PlayerId))));
        break;
    default: break;
    }
}

bool ReviveService::HasOtherPlayers() const noexcept
{
    // The server's player list (it doesn't include us); remote player entities come and go with distance and loads.
    const uint32_t cSelf = m_transport.GetLocalPlayerId();
    for (const auto& [id, name] : m_world.GetPartyService().GetPlayers())
    {
        if (id != cSelf)
            return true;
    }

    // Other players are also entities with a PlayerComponent (skip the local player, 0x14, in case it has one).
    auto view = m_world.view<FormIdComponent, PlayerComponent>();
    for (auto entity : view)
    {
        if (view.get<FormIdComponent>(entity).Id != 0x14)
            return true;
    }
    return false;
}

uint32_t ReviveService::PlayerIdOf(TESObjectREFR* apActor) const noexcept
{
    auto view = m_world.view<FormIdComponent, PlayerComponent>();
    for (auto entity : view)
    {
        if (view.get<FormIdComponent>(entity).Id == apActor->formID)
            return view.get<PlayerComponent>(entity).Id;
    }
    return 0;
}

String ReviveService::NameOf(uint32_t aPlayerId) const noexcept
{
    const auto& players = m_world.GetPartyService().GetPlayers();
    if (auto itor = players.find(aPlayerId); itor != players.end())
        return itor->second;
    return "Your teammate";
}
