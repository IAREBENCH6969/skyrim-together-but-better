#include <Services/PlayerService.h>

#include <World.h>

#include <Events/UpdateEvent.h>
#include <Events/ConnectedEvent.h>
#include <Events/DisconnectedEvent.h>
#include <Events/GridCellChangeEvent.h>
#include <Events/CellChangeEvent.h>
#include <Events/PlayerDialogueEvent.h>
#include <Events/PlayerLevelEvent.h>
#include <Events/PartyJoinedEvent.h>
#include <Events/PartyLeftEvent.h>
#include <Events/BeastFormChangeEvent.h>

#include <Messages/PlayerRespawnRequest.h>
#include <Services/ReviveService.h>
#include <Messages/NotifyPlayerRespawn.h>
#include <Messages/ShiftGridCellRequest.h>
#include <Messages/EnterExteriorCellRequest.h>
#include <Messages/EnterInteriorCellRequest.h>
#include <Messages/PlayerDialogueRequest.h>
#include <Messages/PlayerLevelRequest.h>

#include <Structs/ServerSettings.h>

#include <PlayerCharacter.h>
#include <Forms/TESObjectCELL.h>
#include <Forms/TESGlobal.h>
#include <Games/Overrides.h>
#include <Games/References.h>
#include <AI/AIProcess.h>
#include <EquipManager.h>
#include <Forms/TESRace.h>

PlayerService::PlayerService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_dispatcher(aDispatcher)
    , m_transport(aTransport)
{
    m_updateConnection = m_dispatcher.sink<UpdateEvent>().connect<&PlayerService::OnUpdate>(this);
    m_connectedConnection = m_dispatcher.sink<ConnectedEvent>().connect<&PlayerService::OnConnected>(this);
    m_disconnectedConnection = m_dispatcher.sink<DisconnectedEvent>().connect<&PlayerService::OnDisconnected>(this);
    m_settingsConnection = m_dispatcher.sink<ServerSettings>().connect<&PlayerService::OnServerSettingsReceived>(this);
    m_notifyRespawnConnection = m_dispatcher.sink<NotifyPlayerRespawn>().connect<&PlayerService::OnNotifyPlayerRespawn>(this);
    m_gridCellChangeConnection = m_dispatcher.sink<GridCellChangeEvent>().connect<&PlayerService::OnGridCellChangeEvent>(this);
    m_cellChangeConnection = m_dispatcher.sink<CellChangeEvent>().connect<&PlayerService::OnCellChangeEvent>(this);
    m_playerDialogueConnection = m_dispatcher.sink<PlayerDialogueEvent>().connect<&PlayerService::OnPlayerDialogueEvent>(this);
    m_playerLevelConnection = m_dispatcher.sink<PlayerLevelEvent>().connect<&PlayerService::OnPlayerLevelEvent>(this);
    m_partyJoinedConnection = aDispatcher.sink<PartyJoinedEvent>().connect<&PlayerService::OnPartyJoinedEvent>(this);
    m_partyLeftConnection = aDispatcher.sink<PartyLeftEvent>().connect<&PlayerService::OnPartyLeftEvent>(this);
}

void PlayerService::OnUpdate(const UpdateEvent&) noexcept
{
    RunRespawnUpdates();
    RunPostDeathUpdates();
    RunDifficultyUpdates();
    RunLevelUpdates();
    RunBeastFormDetection();
}

void PlayerService::OnConnected(const ConnectedEvent& acEvent) noexcept
{
    // TODO: SkyrimTogether.esm
    TESGlobal* pKillMove = Cast<TESGlobal>(TESForm::GetById(0x100F19));
    pKillMove->f = 0.f;

    TESGlobal* pWorldEncountersEnabled = Cast<TESGlobal>(TESForm::GetById(0xB8EC1));
    pWorldEncountersEnabled->f = 0.f;
}

void PlayerService::OnDisconnected(const DisconnectedEvent& acEvent) noexcept
{
    PlayerCharacter::Get()->SetDifficulty(m_previousDifficulty);
    m_serverDifficulty = m_previousDifficulty = 6;

    ToggleDeathSystem(false);

    TESGlobal* pKillMove = Cast<TESGlobal>(TESForm::GetById(0x100F19));
    pKillMove->f = 1.f;

    // Restore to the default value (150 in skyrim, 175 in fallout 4)
    float* greetDistance = Settings::GetGreetDistance();
    *greetDistance = 150.f;

    TESGlobal* pWorldEncountersEnabled = Cast<TESGlobal>(TESForm::GetById(0xB8EC1));
    pWorldEncountersEnabled->f = 1.f;
}

void PlayerService::OnServerSettingsReceived(const ServerSettings& acSettings) noexcept
{
    m_previousDifficulty = *Settings::GetDifficulty();
    PlayerCharacter::Get()->SetDifficulty(acSettings.Difficulty);
    m_serverDifficulty = acSettings.Difficulty;

    if (!acSettings.GreetingsEnabled)
    {
        float* greetDistance = Settings::GetGreetDistance();
        *greetDistance = 0.f;
    }

    ToggleDeathSystem(acSettings.DeathSystemEnabled);
}

void PlayerService::OnNotifyPlayerRespawn(const NotifyPlayerRespawn& acMessage) const noexcept
{
    PlayerCharacter::Get()->PayGold(acMessage.GoldLost);

    std::string message = fmt::format("You died and lost {} gold.", acMessage.GoldLost);
    Utils::ShowHudMessage(String(message));
}

void PlayerService::OnGridCellChangeEvent(const GridCellChangeEvent& acEvent) const noexcept
{
    uint32_t baseId = 0;
    uint32_t modId = 0;

    if (m_world.GetModSystem().GetServerModId(acEvent.WorldSpaceId, modId, baseId))
    {
        ShiftGridCellRequest request;
        request.WorldSpaceId = GameId(modId, baseId);
        request.PlayerCell = acEvent.PlayerCell;
        request.CenterCoords = acEvent.CenterCoords;
        request.Cells = acEvent.Cells;

        m_transport.Send(request);
    }
}

void PlayerService::OnCellChangeEvent(const CellChangeEvent& acEvent) const noexcept
{
    if (acEvent.WorldSpaceId)
    {
        EnterExteriorCellRequest message;
        message.CellId = acEvent.CellId;
        message.WorldSpaceId = acEvent.WorldSpaceId;
        message.CurrentCoords = acEvent.CurrentCoords;

        m_transport.Send(message);
    }
    else
    {
        EnterInteriorCellRequest message;
        message.CellId = acEvent.CellId;

        m_transport.Send(message);
    }
}

void PlayerService::OnPlayerDialogueEvent(const PlayerDialogueEvent& acEvent) const noexcept
{
    if (!m_transport.IsConnected())
        return;

    const auto& partyService = m_world.GetPartyService();
    if (!partyService.IsInParty())
        return;

    PlayerDialogueRequest request{};
    request.Text = acEvent.Text;

    m_transport.Send(request);
}

void PlayerService::OnPlayerLevelEvent(const PlayerLevelEvent& acEvent) const noexcept
{
    if (!m_transport.IsConnected())
        return;

    PlayerLevelRequest request{};
    request.NewLevel = PlayerCharacter::Get()->GetLevel();

    m_transport.Send(request);
}

void PlayerService::OnPartyJoinedEvent(const PartyJoinedEvent& acEvent) noexcept
{
    // TODO: this can be done a bit prettier
    if (acEvent.IsLeader)
    {
        TESGlobal* pWorldEncountersEnabled = Cast<TESGlobal>(TESForm::GetById(0xB8EC1));
        pWorldEncountersEnabled->f = 1.f;
    }
}

void PlayerService::OnPartyLeftEvent(const PartyLeftEvent& acEvent) noexcept
{
    // TODO: this can be done a bit prettier
    if (World::Get().GetTransport().IsConnected())
    {
        TESGlobal* pWorldEncountersEnabled = Cast<TESGlobal>(TESForm::GetById(0xB8EC1));
        pWorldEncountersEnabled->f = 0.f;
    }
}

void PlayerService::RunRespawnUpdates() noexcept
{
    if (!m_isDeathSystemEnabled)
        return;

    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    const auto cNow = std::chrono::steady_clock::now();

    if (m_reviving)
    {
        // Revived: once the game has the player standing again, bleedout goes back to unrecoverable for next time.
        if (!pPlayer->actorState.IsBleedingOut())
        {
            pPlayer->SetNoBleedoutRecovery(true);
            m_reviving = false;
            spdlog::info("[PlayerService]: back on your feet after a revive");
        }
        else if (cNow >= m_reviveDeadline && !m_reviveMoved)
        {
            // Still on the ground: moving the player to where they are makes the game stand them up, like a respawn.
            pPlayer->MoveTo(pPlayer->parentCell, pPlayer->position);
            m_reviveMoved = true;
            m_reviveDeadline = cNow + 3s;
        }
        else if (cNow >= m_reviveDeadline)
        {
            spdlog::warn("[PlayerService]: still down after a revive, respawning instead");
            pPlayer->SetNoBleedoutRecovery(true);
            m_reviving = false;
        }
        return;
    }

    if (!pPlayer->actorState.IsBleedingOut())
    {
        m_cachedMainSpellId = pPlayer->magicItems[0] ? pPlayer->magicItems[0]->formID : 0;
        m_cachedSecondarySpellId = pPlayer->magicItems[1] ? pPlayer->magicItems[1]->formID : 0;
        m_cachedPowerId = pPlayer->equippedShout ? pPlayer->equippedShout->formID : 0;

        m_downed = false;
        return;
    }

    auto& revive = m_world.ctx().at<ReviveService>();

    if (!m_downed)
    {
        m_downed = true;
        m_fadeStarted = false;

        // With other players around, wait for one of them to revive this player with a healing potion.
        const float cWindow = m_transport.IsConnected() && revive.HasOtherPlayers() ? revive.GetReviveWindow() : 0.f;
        m_respawnDeadline = cNow + (cWindow > 0.f ? std::chrono::milliseconds(static_cast<int64_t>(cWindow * 1000.f)) : std::chrono::milliseconds(5000));
        spdlog::info("[PlayerService]: down; respawn in {:.0f} s (revive window {:.0f}, other players {})", cWindow > 0.f ? cWindow : 5.f, revive.GetReviveWindow(), revive.HasOtherPlayers());
        if (cWindow > 0.f)
        {
            revive.ReportDowned(true);
            Utils::ShowHudMessage(String(fmt::format("You are down! Another player can revive you with a healing potion. Respawning in {:.0f} seconds", cWindow)));
        }

        // If a player dies not by its health reaching 0, getting it up from its bleedout state isn't possible
        // just by setting its health back to max. Therefore, put it to 0.
        if (pPlayer->GetActorValue(ActorValueInfo::kHealth) > 0.f)
            pPlayer->ForceActorValue(ActorValueOwner::ForceMode::DAMAGE, ActorValueInfo::kHealth, 0);

        pPlayer->PayCrimeGoldToAllFactions();
    }

    if (!m_fadeStarted && cNow >= m_respawnDeadline - 3s)
    {
        FadeOutGame(true, true, 3.0f, true, 2.0f);
        m_fadeStarted = true;
    }

    if (cNow >= m_respawnDeadline)
    {
        spdlog::info("[PlayerService]: nobody revived, respawning");
        pPlayer->RespawnPlayer();

        m_transport.Send(PlayerRespawnRequest());
        revive.ReportDowned(false);

        m_downed = false;

        auto* pEquipManager = EquipManager::Get();
        TESForm* pSpell = TESForm::GetById(m_cachedMainSpellId);
        if (pSpell)
            pEquipManager->EquipSpell(pPlayer, pSpell, 0);
        pSpell = TESForm::GetById(m_cachedSecondarySpellId);
        if (pSpell)
            pEquipManager->EquipSpell(pPlayer, pSpell, 1);
        pSpell = TESForm::GetById(m_cachedPowerId);
        if (pSpell)
            pEquipManager->EquipShout(pPlayer, pSpell);

        m_knockdownDeadline = std::chrono::steady_clock::now() + 1500ms;
        m_knockdownStart = true;
    }
}

bool PlayerService::Revive(float aHealth) noexcept
{
    if (!m_downed || m_reviving)
        return false;

    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    const float cMaxHealth = pPlayer->GetActorPermanentValue(ActorValueInfo::kHealth);
    const float cHealth = std::clamp(aHealth, cMaxHealth * 0.25f, cMaxHealth);

    // Bleedout recovery is turned off while connected (the death system); allow it until the player is standing.
    pPlayer->SetNoBleedoutRecovery(false);
    pPlayer->ForceActorValue(ActorValueOwner::ForceMode::DAMAGE, ActorValueInfo::kHealth, cHealth);
    if (m_fadeStarted)
        FadeOutGame(false, true, 0.5f, true, 0.5f);

    m_downed = false;
    m_fadeStarted = false;
    m_reviving = true;
    m_reviveMoved = false;
    m_reviveDeadline = std::chrono::steady_clock::now() + 3s;
    m_revives++;
    m_world.ctx().at<ReviveService>().ReportDowned(false);

    spdlog::info("[PlayerService]: revived with {:.0f} of {:.0f} health", cHealth, cMaxHealth);
    return true;
}

// Doesn't seem to respawn quite yet
void PlayerService::RunPostDeathUpdates() noexcept
{
    if (!m_isDeathSystemEnabled)
        return;

    // If a player dies in ragdoll, it gets stuck.
    // This code ragdolls the player again upon respawning.
    // It also makes the player invincible for 10 seconds.
    const auto cNow = std::chrono::steady_clock::now();
    if (m_knockdownStart && cNow >= m_knockdownDeadline)
    {
        PlayerCharacter* pPlayer = PlayerCharacter::Get();

        PlayerCharacter::SetGodMode(true);
        m_godmodeStart = true;

        pPlayer->currentProcess->KnockExplosion(pPlayer, &pPlayer->position, 0.f);

        FadeOutGame(false, true, 0.5f, true, 2.f);

        m_godmodeDeadline = std::chrono::steady_clock::now() + 10s;
        m_knockdownStart = false;
    }

    if (m_godmodeStart && cNow >= m_godmodeDeadline)
    {
        PlayerCharacter::SetGodMode(false);

        m_godmodeStart = false;
    }
}

void PlayerService::RunDifficultyUpdates() const noexcept
{
    if (!m_transport.IsConnected())
        return;

    PlayerCharacter::Get()->SetDifficulty(m_serverDifficulty);
}

void PlayerService::RunLevelUpdates() const noexcept
{
    // The LevelUp hook is kinda weird, so ehh, just check periodically, doesn't really cost anything.

    static std::chrono::steady_clock::time_point lastSendTimePoint;
    constexpr auto cDelayBetweenUpdates = 1000ms;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSendTimePoint < cDelayBetweenUpdates)
        return;

    lastSendTimePoint = now;

    static uint16_t oldLevel = PlayerCharacter::Get()->GetLevel();

    uint16_t newLevel = PlayerCharacter::Get()->GetLevel();
    if (newLevel != oldLevel)
    {
        PlayerLevelRequest request{};
        request.NewLevel = newLevel;

        m_transport.Send(request);

        oldLevel = newLevel;
    }
}

void PlayerService::RunBeastFormDetection() const noexcept
{
    static uint32_t lastRaceFormID = 0;
    static std::chrono::steady_clock::time_point lastSendTimePoint;
    constexpr auto cDelayBetweenUpdates = 250ms;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSendTimePoint < cDelayBetweenUpdates)
        return;

    lastSendTimePoint = now;

    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pPlayer->race)
        return;

    if (pPlayer->race->formID == lastRaceFormID)
        return;

    if (pPlayer->race->formID == 0x200283A || pPlayer->race->formID == 0xCDD84)
        m_world.GetDispatcher().trigger(BeastFormChangeEvent());

    lastRaceFormID = pPlayer->race->formID;
}

void PlayerService::ToggleDeathSystem(bool aSet) noexcept
{
    m_isDeathSystemEnabled = aSet;

    PlayerCharacter::Get()->SetPlayerRespawnMode(aSet);
}
