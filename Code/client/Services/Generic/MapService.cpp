#include <Services/MapService.h>

#include <World.h>

#include <Events/DisconnectedEvent.h>
#include <Events/SetWaypointEvent.h>
#include <Events/RemoveWaypointEvent.h>
#include <Messages/NotifySetWaypoint.h>
#include <Messages/NotifyRemoveWaypoint.h>
#include <Messages/RequestSetWaypoint.h>
#include <Messages/RequestRemoveWaypoint.h>
#include <Services/ImguiService.h>
#include <Services/PartyService.h>

#include <BSGraphics/BSGraphicsRenderer.h>
#include <Camera/PlayerCamera.h>
#include <Interface/UI.h>
#include <PlayerCharacter.h>
#include <Forms/TESObjectCELL.h>
#include <Forms/TESWorldSpace.h>

#include <NetImmerse/NiCamera.h>

#include <imgui.h>

namespace
{
// Where the HUD compass is, as fractions of the screen, and how many degrees each half of it spans. Measured against
// the game's own custom marker icon at 16:9.
constexpr float kCompassCenterY = 0.0545f;
constexpr float kCompassHalfWidth = 0.1335f;
constexpr float kCompassHalfAngle = 57.f;

// NiCamera: world-to-camera matrix and viewport, used to project a world point onto that camera's screen.
constexpr size_t kCameraWorldToCam = 0x110;
constexpr size_t kCameraPort = 0x174;
// NiAVObject: world transform's rotation (3x3, row-major).
constexpr size_t kObjectWorldRotate = 0x7C;

// The game camera.
NiCamera* MainCamera() noexcept
{
    PlayerCamera* pCamera = PlayerCamera::Get();
    return pCamera ? pCamera->GetNiCamera() : nullptr;
}

// Heading of a camera (radians clockwise from north): it looks along its local X axis.
float CameraHeading(const NiCamera* apCamera) noexcept
{
    const float* pRotate = reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(apCamera) + kObjectWorldRotate);
    // Column 0 of the world rotation is where local X points in the world.
    return std::atan2(pRotate[0], pRotate[3]);
}

bool ProjectWithCamera(const NiCamera* apCamera, const NiPoint3& acPoint, NiPoint3& aScreen) noexcept
{
    auto* pBase = reinterpret_cast<uint8_t*>(const_cast<NiCamera*>(apCamera));
    return NiCamera::WorldPtToScreenPt3(reinterpret_cast<float*>(pBase + kCameraWorldToCam), reinterpret_cast<const NiRect<float>*>(pBase + kCameraPort), &acPoint, &aScreen.x, &aScreen.y,
                                        &aScreen.z, 1e-5f);
}

// One colour per player, readable on the compass and the map.
ImU32 PlayerColor(uint32_t aPlayerId) noexcept
{
    static constexpr ImU32 s_colors[] = {IM_COL32(64, 170, 255, 255), IM_COL32(255, 196, 64, 255), IM_COL32(120, 220, 110, 255),
                                         IM_COL32(235, 110, 220, 255), IM_COL32(255, 130, 80, 255), IM_COL32(150, 130, 255, 255)};
    return s_colors[aPlayerId % (sizeof(s_colors) / sizeof(s_colors[0]))];
}

// A map pin with its tip at acTip.
void DrawPin(ImDrawList* apDraw, ImVec2 acTip, float aSize, ImU32 aColor) noexcept
{
    const ImVec2 cHead{acTip.x, acTip.y - aSize * 1.25f};
    const float cRadius = aSize * 0.62f;
    apDraw->AddCircleFilled({cHead.x + 1.f, cHead.y + 1.5f}, cRadius + 1.5f, IM_COL32(0, 0, 0, 140), 24);
    apDraw->AddTriangleFilled({cHead.x - cRadius * 0.8f, cHead.y + cRadius * 0.45f}, {cHead.x + cRadius * 0.8f, cHead.y + cRadius * 0.45f}, acTip, aColor);
    apDraw->AddCircleFilled(cHead, cRadius, aColor, 24);
    apDraw->AddCircle(cHead, cRadius, IM_COL32(20, 18, 16, 230), 24, 1.5f);
    apDraw->AddLine({cHead.x - cRadius * 0.8f, cHead.y + cRadius * 0.45f}, acTip, IM_COL32(20, 18, 16, 230), 1.5f);
    apDraw->AddLine({cHead.x + cRadius * 0.8f, cHead.y + cRadius * 0.45f}, acTip, IM_COL32(20, 18, 16, 230), 1.5f);
    apDraw->AddCircleFilled(cHead, cRadius * 0.38f, IM_COL32(245, 242, 232, 255), 16);
}

void DrawLabel(ImDrawList* apDraw, ImVec2 acCenterTop, const char* acpText, float aSize, ImU32 aColor) noexcept
{
    ImFont* pFont = ImGui::GetFont();
    const ImVec2 cSize = pFont->CalcTextSizeA(aSize, FLT_MAX, 0.f, acpText);
    const ImVec2 cPos{acCenterTop.x - cSize.x * 0.5f, acCenterTop.y};
    apDraw->AddText(pFont, aSize, {cPos.x + 1.f, cPos.y + 1.f}, IM_COL32(0, 0, 0, 230), acpText);
    apDraw->AddText(pFont, aSize, cPos, aColor, acpText);
}

String NameOf(uint32_t aPlayerId) noexcept
{
    const auto& players = World::Get().GetPartyService().GetPlayers();
    if (auto itor = players.find(aPlayerId); itor != players.end())
        return itor->second;
    return "Player";
}

// The worldspace the local player is in, or 0 inside.
uint32_t CurrentWorldSpaceId() noexcept
{
    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    TESWorldSpace* pWorldSpace = pPlayer ? pPlayer->GetWorldSpace() : nullptr;
    return pWorldSpace ? pWorldSpace->formID : 0;
}
} // namespace

MapService::MapService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_dispatcher(aDispatcher)
    , m_transport(aTransport)
{
    m_playerNotifySetWaypointConnection = m_dispatcher.sink<NotifySetWaypoint>().connect<&MapService::OnNotifySetWaypoint>(this);
    m_playerNotifyRemoveWaypointConnection = m_dispatcher.sink<NotifyRemoveWaypoint>().connect<&MapService::OnNotifyRemoveWaypoint>(this);
    m_playerSetWaypointConnection = m_dispatcher.sink<SetWaypointEvent>().connect<&MapService::OnSetWaypoint>(this);
    m_playerRemoveWaypointConnection = m_dispatcher.sink<RemoveWaypointEvent>().connect<&MapService::OnRemoveWaypoint>(this);
    m_disconnectedConnection = m_dispatcher.sink<DisconnectedEvent>().connect<&MapService::OnDisconnected>(this);
    m_drawConnection = aWorld.ctx().at<ImguiService>().OnDraw.connect<&MapService::OnDraw>(this);
}

void MapService::OnSetWaypoint(const SetWaypointEvent& acMessage) noexcept
{
    if (!m_transport.IsConnected())
        return;

    RequestSetWaypoint request{};
    request.Position = acMessage.Position;

    ModSystem& modSystem = m_world.Get().GetModSystem();
    if (!modSystem.GetServerModId(acMessage.WorldSpaceFormID, request.WorldSpaceFormID))
        return;

    m_transport.Send(request);
}

void MapService::OnRemoveWaypoint(const RemoveWaypointEvent& acMessage) noexcept
{
    if (!m_transport.IsConnected())
        return;

    RequestRemoveWaypoint request{};
    m_transport.Send(request);
}

void MapService::OnNotifySetWaypoint(const NotifySetWaypoint& acMessage) noexcept
{
    ModSystem& modSystem = m_world.Get().GetModSystem();
    const uint32_t cLocationId = modSystem.GetGameId(acMessage.WorldSpaceFormID);

    // A marker in a worldspace or cell this client doesn't have can't be shown anywhere.
    TESForm* pLocation = TESForm::GetById(cLocationId);
    if (!Cast<TESWorldSpace>(pLocation) && !Cast<TESObjectCELL>(pLocation))
    {
        spdlog::warn("{}: no worldspace or cell {:X} for player {}'s marker, skipping it", __FUNCTION__, cLocationId, acMessage.PlayerId);
        return;
    }

    Marker marker{};
    marker.Position.x = acMessage.Position.x;
    marker.Position.y = acMessage.Position.y;
    marker.Position.z = acMessage.Position.z;
    marker.WorldSpaceId = cLocationId;
    m_markers[acMessage.PlayerId] = marker;
    spdlog::info("[MapService]: player {} placed a map marker at ({:.0f}, {:.0f}) in {:X}", acMessage.PlayerId, marker.Position.x, marker.Position.y, cLocationId);
}

void MapService::OnNotifyRemoveWaypoint(const NotifyRemoveWaypoint& acMessage) noexcept
{
    if (m_markers.erase(acMessage.PlayerId))
        spdlog::info("[MapService]: player {} removed their map marker", acMessage.PlayerId);
}

void MapService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    m_markers.clear();
}

void MapService::OnDraw() noexcept
{
    if (m_markers.empty() || !PlayerCharacter::Get())
        return;
    UI* pUI = UI::Get();
    if (!pUI)
        return;

    if (pUI->GetMenuOpen("MapMenu"))
        DrawOnMap();
    else if (pUI->numPausesGame == 0 && !pUI->GetMenuOpen("Dialogue Menu") && !pUI->GetMenuOpen("Console"))
        DrawOnCompass();
}

void MapService::DrawOnCompass() noexcept
{
    auto* pWindow = BSGraphics::GetMainWindow();
    PlayerCamera* pCamera = PlayerCamera::Get();
    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    const uint32_t cWorldSpaceId = CurrentWorldSpaceId();
    if (!pWindow || !pCamera || !cWorldSpaceId)
        return; // inside, the compass doesn't point at outside markers either

    const float cWidth = static_cast<float>(pWindow->uiWindowWidth);
    const float cHeight = static_cast<float>(pWindow->uiWindowHeight);
    const float cScale = std::max(0.75f, cHeight / 1080.f);
    const float cCenterX = cWidth * 0.5f;
    const float cCompassY = cHeight * kCompassCenterY;
    const float cHalfWidth = cWidth * kCompassHalfWidth;
    ImDrawList* pDraw = ImGui::GetBackgroundDrawList();

    // The compass follows the camera's heading.
    NiCamera* pNiCamera = pCamera->GetNiCamera();
    if (!pNiCamera)
        return;
    const float cHeading = CameraHeading(pNiCamera);
    for (const auto& [playerId, marker] : m_markers)
    {
        if (marker.WorldSpaceId != cWorldSpaceId)
            continue;
        const float cBearing = std::atan2(marker.Position.x - pPlayer->position.x, marker.Position.y - pPlayer->position.y);
        float delta = (cBearing - cHeading) * 180.f / 3.14159265f;
        while (delta > 180.f)
            delta -= 360.f;
        while (delta < -180.f)
            delta += 360.f;
        if (std::abs(delta) > kCompassHalfAngle)
            continue;

        const float cX = cCenterX + delta / kCompassHalfAngle * cHalfWidth;
        const ImU32 cColor = PlayerColor(playerId);
        DrawPin(pDraw, {cX, cCompassY + 6.f * cScale}, 9.f * cScale, cColor);
        // Name and distance under the compass while it's roughly straight ahead.
        if (std::abs(delta) < 12.f)
        {
            const float cDx = marker.Position.x - pPlayer->position.x;
            const float cDy = marker.Position.y - pPlayer->position.y;
            const int cMeters = static_cast<int>(std::sqrt(cDx * cDx + cDy * cDy) / 70.f);
            DrawLabel(pDraw, {cX, cCompassY + 12.f * cScale}, fmt::format("{}  {} m", NameOf(playerId).c_str(), cMeters).c_str(), ImGui::GetFontSize() * 1.15f * cScale, cColor);
        }
    }
}

void MapService::DrawOnMap() noexcept
{
    auto* pWindow = BSGraphics::GetMainWindow();
    if (!pWindow)
        return;
    const float cWidth = static_cast<float>(pWindow->uiWindowWidth);
    const float cHeight = static_cast<float>(pWindow->uiWindowHeight);
    const float cScale = std::max(0.75f, cHeight / 1080.f);
    const uint32_t cWorldSpaceId = CurrentWorldSpaceId();
    ImDrawList* pDraw = ImGui::GetBackgroundDrawList();
    // The world map is drawn through the main camera, moved above the map.
    NiCamera* pMapCamera = MainCamera();
    if (!pMapCamera)
        return;

    for (const auto& [playerId, marker] : m_markers)
    {
        // The world map shows the worldspace the player is in (or the one outside, when inside).
        if (cWorldSpaceId && marker.WorldSpaceId != cWorldSpaceId)
            continue;

        NiPoint3 screen{};
        if (!ProjectWithCamera(pMapCamera, marker.Position, screen))
            continue;
        if (screen.z < 0.f || screen.x < 0.f || screen.x > 1.f || screen.y < 0.f || screen.y > 1.f)
            continue;

        const ImVec2 cTip{cWidth * screen.x, cHeight * (1.f - screen.y)};
        const ImU32 cColor = PlayerColor(playerId);
        const float cPinSize = 13.f * cScale;
        const float cTextSize = ImGui::GetFontSize() * 1.2f * cScale;
        DrawPin(pDraw, cTip, cPinSize, cColor);
        // Above the pin: below it is where the map writes the player's own "Current Location".
        DrawLabel(pDraw, {cTip.x, cTip.y - cPinSize * 2.f - cTextSize - 2.f * cScale}, fmt::format("{}'s marker", NameOf(playerId).c_str()).c_str(), cTextSize, cColor);
    }
}
