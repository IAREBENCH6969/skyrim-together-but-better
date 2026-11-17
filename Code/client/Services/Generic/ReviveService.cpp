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
#include <AI/Movement/PlayerControls.h>
#include <Services/OverlayService.h>
#include <Services/ImguiService.h>
#include <BSGraphics/BSGraphicsRenderer.h>
#include <Interface/Menus/HUDMenuUtils.h>
#include <OverlayApp.hpp>

#include <imgui.h>

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

void ExecuteUi(const char* acpEvent, CefRefPtr<CefListValue> apArguments) noexcept
{
    if (auto* pApp = World::Get().GetOverlayService().GetOverlayApp())
        pApp->ExecuteAsync(acpEvent, apArguments);
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
    m_drawConnection = aWorld.ctx().at<ImguiService>().OnDraw.connect<&ReviveService::OnDraw>(this);
}

void ReviveService::ReportDowned(bool aDowned, float aSeconds) noexcept
{
    if (!m_transport.IsConnected() || m_reviveWindow <= 0.f)
        return;

    ReviveRequest request;
    request.Action = aDowned ? ReviveAction::Downed : ReviveAction::Up;
    request.Seconds = aSeconds;
    m_transport.Send(request);

    // The countdown in the overlay ("reviveUp" with a name shows who revived us).
    auto pArguments = CefListValue::Create();
    if (aDowned)
    {
        pArguments->SetDouble(0, aSeconds);
        ExecuteUi("reviveDowned", pArguments);
    }
    else
    {
        pArguments->SetString(0, "");
        ExecuteUi("reviveUp", pArguments);
    }
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
    const bool cActivateDown = (GetAsyncKeyState(kActivateKey) & 0x8000) != 0;

    // Give the game its activation back a moment after E was let go.
    if (m_activationBlocked)
    {
        const auto cNow = std::chrono::steady_clock::now();
        if (cActivateDown)
            m_unblockAt = cNow + 300ms;
        else if (cNow >= m_unblockAt)
        {
            if (PlayerControls* pControls = PlayerControls::GetInstance())
                pControls->SetActivateEnabled(true);
            m_activationBlocked = false;
        }
    }

    if (m_downedPlayers.empty())
    {
        m_activateWasDown = cActivateDown;
        return;
    }

    // A fresh press of Activate next to a downed player revives them; that press is used up.
    if (cActivateDown && !m_activateWasDown && PlayerIsInGame() && TryReviveNearby())
    {
        if (PlayerControls* pControls = PlayerControls::GetInstance())
        {
            pControls->SetActivateEnabled(false);
            m_activationBlocked = true;
            m_unblockAt = std::chrono::steady_clock::now() + 300ms;
        }
    }
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
        m_downedPlayers[acMessage.PlayerId] = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int64_t>(acMessage.Seconds * 1000.f));
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
        {
            auto pArguments = CefListValue::Create();
            pArguments->SetString(0, NameOf(acMessage.PlayerId).c_str());
            ExecuteUi("reviveUp", pArguments);
        }
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

void ReviveService::OnDraw() noexcept
{
    if (m_downedPlayers.empty())
        return;
    UI* pUI = UI::Get();
    if (!pUI || pUI->numPausesGame > 0) // not over the inventory, map, pause menu...
        return;
    auto* pWindow = BSGraphics::GetMainWindow();
    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pWindow || !pPlayer)
        return;

    const float cWidth = static_cast<float>(pWindow->uiWindowWidth);
    const float cHeight = static_cast<float>(pWindow->uiWindowHeight);
    const float cScale = std::max(0.75f, cHeight / 1080.f);
    const float cMargin = 48.f * cScale;
    const auto cNow = std::chrono::steady_clock::now();
    const float cClock = std::chrono::duration<float>(cNow.time_since_epoch()).count();

    ImDrawList* pDraw = ImGui::GetBackgroundDrawList();
    ImFont* pFont = ImGui::GetFont();
    const float cFontSize = ImGui::GetFontSize() * 1.25f * cScale;

    auto view = m_world.view<FormIdComponent, PlayerComponent>();
    for (auto entity : view)
    {
        const auto cDowned = m_downedPlayers.find(view.get<PlayerComponent>(entity).Id);
        if (cDowned == m_downedPlayers.end())
            continue;
        auto* pActor = Cast<Actor>(TESForm::GetById(view.get<FormIdComponent>(entity).Id));
        if (!pActor || pActor == pPlayer || pActor->GetParentCellEx() != pPlayer->GetParentCellEx())
            continue;

        // Above the kneeling body.
        NiPoint3 world = pActor->position;
        world.z += 105.f;
        NiPoint3 screen{};
        HUDMenuUtils::WorldPtToScreenPt3(world, screen);

        ImVec2 point{cWidth * screen.x, cHeight * (1.f - screen.y)};
        const bool cBehind = screen.z < 0.f;
        if (cBehind) // the projection mirrors points behind the camera
            point = {cWidth - point.x, cHeight - point.y};
        const bool cOnScreen = !cBehind && point.x >= cMargin && point.x <= cWidth - cMargin && point.y >= cMargin && point.y <= cHeight - cMargin;
        if (!cOnScreen)
        {
            // Off screen: pin it to the edge, in their direction, so the player knows which way to turn.
            point.x = std::clamp(point.x, cMargin, cWidth - cMargin);
            point.y = cBehind ? cHeight - cMargin : std::clamp(point.y, cMargin, cHeight - cMargin);
        }

        const float cDx = pActor->position.x - pPlayer->position.x;
        const float cDy = pActor->position.y - pPlayer->position.y;
        const float cDz = pActor->position.z - pPlayer->position.z;
        const int cMeters = static_cast<int>(std::sqrt(cDx * cDx + cDy * cDy + cDz * cDz) / 70.f);
        const int cSecondsLeft = static_cast<int>(std::max(0.f, std::chrono::duration<float>(cDowned->second - cNow).count()) + 0.99f);

        const float cRadius = (cOnScreen ? 17.f : 14.f) * cScale;
        const float cPulse = std::fmod(cClock, 1.4f) / 1.4f;
        const bool cUrgent = cSecondsLeft <= 10;
        const ImU32 cCross = cUrgent && std::fmod(cClock, 0.6f) < 0.3f ? IM_COL32(255, 120, 100, 255) : IM_COL32(200, 40, 35, 255);

        // Expanding ring to catch the eye, then the marker: dark disc, pale rim, red cross.
        pDraw->AddCircle(point, cRadius + cPulse * 16.f * cScale, IM_COL32(210, 50, 40, static_cast<int>(190.f * (1.f - cPulse))), 40, 2.5f * cScale);
        pDraw->AddCircleFilled({point.x + 1.5f * cScale, point.y + 2.5f * cScale}, cRadius + 2.f * cScale, IM_COL32(0, 0, 0, 120), 40);
        pDraw->AddCircleFilled(point, cRadius, IM_COL32(20, 18, 16, 225), 40);
        pDraw->AddCircle(point, cRadius, IM_COL32(232, 226, 210, 255), 40, 2.f * cScale);
        const float cLong = cRadius * 0.58f;
        const float cShort = cRadius * 0.2f;
        pDraw->AddRectFilled({point.x - cShort, point.y - cLong}, {point.x + cShort, point.y + cLong}, cCross, 1.5f * cScale);
        pDraw->AddRectFilled({point.x - cLong, point.y - cShort}, {point.x + cLong, point.y + cShort}, cCross, 1.5f * cScale);

        // Name, distance and time left underneath, with a shadow so it reads on any background.
        const std::string cLabel = fmt::format("{}  {} m  {} s", NameOf(cDowned->first).c_str(), cMeters, cSecondsLeft);
        const ImVec2 cSize = pFont->CalcTextSizeA(cFontSize, FLT_MAX, 0.f, cLabel.c_str());
        ImVec2 text{point.x - cSize.x * 0.5f, point.y + cRadius + 6.f * cScale};
        text.x = std::clamp(text.x, 4.f, cWidth - cSize.x - 4.f);
        text.y = std::min(text.y, cHeight - cSize.y - 4.f);
        pDraw->AddText(pFont, cFontSize, {text.x + 1.f, text.y + 1.f}, IM_COL32(0, 0, 0, 220), cLabel.c_str());
        pDraw->AddText(pFont, cFontSize, text, cUrgent ? IM_COL32(255, 170, 150, 255) : IM_COL32(240, 236, 224, 255), cLabel.c_str());
    }
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
