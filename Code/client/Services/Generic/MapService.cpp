#include <Services/MapService.h>

#include <World.h>

#include <Events/DisconnectedEvent.h>
#include <Events/SetWaypointEvent.h>
#include <Events/RemoveWaypointEvent.h>
#include <Messages/NotifySetWaypoint.h>
#include <Messages/NotifyRemoveWaypoint.h>
#include <Messages/NotifyPlayerPositions.h>
#include <Messages/RequestSetWaypoint.h>
#include <Messages/RequestRemoveWaypoint.h>
#include <Services/ImguiService.h>
#include <Services/PartyService.h>
#include <Components.h>
#include <Actor.h>

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

// Player colours, muted like Skyrim's own UI (no neon), readable on the compass and the snowy map, and far enough
// apart to tell twelve players apart.
constexpr ImU32 kPlayerColors[] = {
    IM_COL32(212, 175, 92, 255),  // old gold
    IM_COL32(112, 164, 204, 255), // steel blue
    IM_COL32(196, 92, 78, 255),   // dragon's-blood red
    IM_COL32(128, 172, 104, 255), // moss green
    IM_COL32(168, 128, 196, 255), // amethyst
    IM_COL32(204, 134, 82, 255),  // copper
    IM_COL32(86, 168, 156, 255),  // verdigris
    IM_COL32(204, 120, 152, 255), // rose
    IM_COL32(222, 212, 182, 255), // bone
    IM_COL32(106, 112, 196, 255), // lapis
    IM_COL32(168, 166, 82, 255),  // olive
    IM_COL32(150, 200, 226, 255), // frost
};
constexpr size_t kPlayerColorCount = sizeof(kPlayerColors) / sizeof(kPlayerColors[0]);

// Skyrim's text colour (off-white) and the near-black of its icon outlines.
constexpr ImU32 kTextColor = IM_COL32(236, 232, 220, 255);
constexpr ImU32 kOutlineColor = IM_COL32(16, 14, 12, 235);

ImU32 WithAlpha(ImU32 aColor, float aAlpha) noexcept
{
    const uint32_t cAlpha = static_cast<uint32_t>(((aColor >> IM_COL32_A_SHIFT) & 0xFF) * std::clamp(aAlpha, 0.f, 1.f));
    return (aColor & ~IM_COL32_A_MASK) | (cAlpha << IM_COL32_A_SHIFT);
}

// aColor blended towards white (aAmount > 0) or black (aAmount < 0).
ImU32 Shade(ImU32 aColor, float aAmount) noexcept
{
    const ImVec4 c = ImGui::ColorConvertU32ToFloat4(aColor);
    const float cTarget = aAmount > 0.f ? 1.f : 0.f;
    const float t = std::abs(aAmount);
    return ImGui::ColorConvertFloat4ToU32({c.x + (cTarget - c.x) * t, c.y + (cTarget - c.y) * t, c.z + (cTarget - c.z) * t, c.w});
}

ImVec2 Add(ImVec2 a, ImVec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
ImVec2 Mul(ImVec2 a, float f) noexcept { return {a.x * f, a.y * f}; }

// A marker shaped like the game's own custom marker: a long kite pointing down at acTip, outlined in near-black,
// with a lighter inner facet. aHeight is the whole icon's height.
void DrawKite(ImDrawList* apDraw, ImVec2 acTip, float aHeight, ImU32 aColor, float aAlpha = 1.f) noexcept
{
    const float h = aHeight;
    const ImVec2 cTop{acTip.x, acTip.y - h};
    const ImVec2 cLeft{acTip.x - h * 0.34f, acTip.y - h * 0.64f};
    const ImVec2 cRight{acTip.x + h * 0.34f, acTip.y - h * 0.64f};
    const float o = std::max(1.5f, h * 0.07f); // outline width

    // Soft shadow, then the dark outline (a slightly bigger kite), then the colour.
    const ImVec2 cShadow{1.f, 2.f};
    const ImVec2 cShadowKite[] = {Add(cTop, cShadow), Add(cRight, cShadow), Add(acTip, cShadow), Add(cLeft, cShadow)};
    apDraw->AddConvexPolyFilled(cShadowKite, 4, WithAlpha(IM_COL32(0, 0, 0, 110), aAlpha));
    const ImVec2 cOuter[] = {{cTop.x, cTop.y - o}, {cRight.x + o, cRight.y}, {acTip.x, acTip.y + o * 1.4f}, {cLeft.x - o, cLeft.y}};
    apDraw->AddConvexPolyFilled(cOuter, 4, WithAlpha(kOutlineColor, aAlpha));
    const ImVec2 cKite[] = {cTop, cRight, acTip, cLeft};
    apDraw->AddConvexPolyFilled(cKite, 4, WithAlpha(aColor, aAlpha));

    // Left half lit, right half in shade: the bevel Skyrim's icons have.
    const ImVec2 cMid{acTip.x, acTip.y - h * 0.56f};
    const ImVec2 cLit[] = {cTop, cMid, acTip, cLeft};
    apDraw->AddConvexPolyFilled(cLit, 4, WithAlpha(Shade(aColor, 0.28f), aAlpha));
    const ImVec2 cDark[] = {cTop, cRight, acTip, cMid};
    apDraw->AddConvexPolyFilled(cDark, 4, WithAlpha(Shade(aColor, -0.22f), aAlpha));

    // The inner "V" facet, like the native marker's.
    const ImVec2 cInner[] = {{acTip.x, acTip.y - h * 0.82f}, {acTip.x + h * 0.15f, acTip.y - h * 0.6f}, {acTip.x, acTip.y - h * 0.24f}, {acTip.x - h * 0.15f, acTip.y - h * 0.6f}};
    apDraw->AddConvexPolyFilled(cInner, 4, WithAlpha(Shade(aColor, -0.45f), aAlpha));
    apDraw->AddPolyline(cInner, 4, WithAlpha(Shade(aColor, 0.55f), aAlpha * 0.9f), ImDrawFlags_Closed, std::max(1.f, h * 0.035f));
}

// Another player, like the game's own player arrow: an arrowhead with a notched tail, pointing along acDirection
// (a unit vector on screen), centred on acCenter. aSize is the length from the centre to the tip.
void DrawPlayerArrow(ImDrawList* apDraw, ImVec2 acCenter, ImVec2 acDirection, float aSize, ImU32 aColor, float aAlpha) noexcept
{
    const ImVec2 d = acDirection;
    const ImVec2 p{-d.y, d.x}; // perpendicular
    const ImVec2 cTip = Add(acCenter, Mul(d, aSize));
    const ImVec2 cBackLeft = Add(acCenter, Add(Mul(d, -aSize * 0.62f), Mul(p, -aSize * 0.62f)));
    const ImVec2 cBackRight = Add(acCenter, Add(Mul(d, -aSize * 0.62f), Mul(p, aSize * 0.62f)));
    const ImVec2 cNotch = Add(acCenter, Mul(d, -aSize * 0.22f));
    const ImVec2 cOutline[] = {cTip, cBackRight, cNotch, cBackLeft};
    const float cWidth = std::max(1.5f, aSize * 0.14f);

    // Shadow and thick dark outline under the fill (the shape is notched, so it is filled as two halves).
    const ImVec2 cShadow{1.f, 2.f};
    const ImVec2 cShadowShape[] = {Add(cTip, cShadow), Add(cBackRight, cShadow), Add(cNotch, cShadow), Add(cBackLeft, cShadow)};
    apDraw->AddPolyline(cShadowShape, 4, WithAlpha(IM_COL32(0, 0, 0, 110), aAlpha), ImDrawFlags_Closed, cWidth * 1.6f);
    apDraw->AddPolyline(cOutline, 4, WithAlpha(kOutlineColor, aAlpha), ImDrawFlags_Closed, cWidth * 1.6f);
    apDraw->AddTriangleFilled(cTip, cNotch, cBackLeft, WithAlpha(Shade(aColor, 0.3f), aAlpha));
    apDraw->AddTriangleFilled(cTip, cBackRight, cNotch, WithAlpha(Shade(aColor, -0.2f), aAlpha));
    apDraw->AddPolyline(cOutline, 4, WithAlpha(kOutlineColor, aAlpha), ImDrawFlags_Closed, std::max(1.f, cWidth * 0.5f));
}

// Another player on the compass: a round gem like Skyrim's enemy dots, outlined, lit from the top left.
void DrawPlayerDot(ImDrawList* apDraw, ImVec2 acCenter, float aRadius, ImU32 aColor, float aAlpha) noexcept
{
    apDraw->AddCircleFilled({acCenter.x + 1.f, acCenter.y + 1.5f}, aRadius + 2.f, WithAlpha(IM_COL32(0, 0, 0, 110), aAlpha), 24);
    apDraw->AddCircleFilled(acCenter, aRadius + std::max(1.5f, aRadius * 0.22f), WithAlpha(kOutlineColor, aAlpha), 24);
    apDraw->AddCircleFilled(acCenter, aRadius, WithAlpha(Shade(aColor, -0.2f), aAlpha), 24);
    apDraw->AddCircleFilled({acCenter.x - aRadius * 0.12f, acCenter.y - aRadius * 0.12f}, aRadius * 0.78f, WithAlpha(aColor, aAlpha), 24);
    apDraw->AddCircleFilled({acCenter.x - aRadius * 0.32f, acCenter.y - aRadius * 0.32f}, aRadius * 0.28f, WithAlpha(Shade(aColor, 0.6f), aAlpha * 0.9f), 16);
}

ImFont* ThemeFont() noexcept
{
    ImFont* pFont = World::Get().ctx().at<ImguiService>().GetThemeFont();
    return pFont ? pFont : ImGui::GetFont();
}

enum class Anchor
{
    Above, // text sits above the point
    Below  // text hangs below the point
};

// Text like Skyrim's map and HUD labels: off-white Futura Condensed with a soft dark edge, centred on acPoint.
void DrawSkyrimText(ImDrawList* apDraw, ImVec2 acPoint, Anchor aAnchor, const std::string& acText, float aSize, ImU32 aColor = kTextColor, float aAlpha = 1.f) noexcept
{
    ImFont* pFont = ThemeFont();
    const ImVec2 cSize = pFont->CalcTextSizeA(aSize, FLT_MAX, 0.f, acText.c_str());
    const ImVec2 cPos{std::round(acPoint.x - cSize.x * 0.5f), std::round(aAnchor == Anchor::Above ? acPoint.y - cSize.y : acPoint.y)};
    const ImU32 cEdge = WithAlpha(IM_COL32(0, 0, 0, 150), aAlpha);
    for (const ImVec2 cOffset : {ImVec2{-1.f, 0.f}, ImVec2{1.f, 0.f}, ImVec2{0.f, -1.f}, ImVec2{0.f, 1.f}, ImVec2{1.5f, 1.5f}})
        apDraw->AddText(pFont, aSize, Add(cPos, cOffset), cEdge, acText.c_str());
    apDraw->AddText(pFont, aSize, cPos, WithAlpha(aColor, aAlpha), acText.c_str());
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
    m_playerPositionsConnection = m_dispatcher.sink<NotifyPlayerPositions>().connect<&MapService::OnNotifyPlayerPositions>(this);
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
    m_players.clear();
    m_colorIndex.clear();
}

uint32_t MapService::ColorOf(uint32_t aPlayerId) const noexcept
{
    const auto itor = m_colorIndex.find(aPlayerId);
    const size_t cIndex = itor != m_colorIndex.end() ? itor->second : aPlayerId; // until the server says
    return kPlayerColors[cIndex % kPlayerColorCount];
}

int32_t MapService::GetColorIndex(uint32_t aPlayerId) const noexcept
{
    const auto itor = m_colorIndex.find(aPlayerId);
    return itor != m_colorIndex.end() ? static_cast<int32_t>(itor->second) : -1;
}

void MapService::OnNotifyPlayerPositions(const NotifyPlayerPositions& acMessage) noexcept
{
    ModSystem& modSystem = m_world.Get().GetModSystem();
    const uint32_t cSelf = m_transport.GetLocalPlayerId();

    TiltedPhoques::Map<uint32_t, PlayerOnMap> players;
    for (const auto& entry : acMessage.Players)
    {
        m_colorIndex[entry.PlayerId] = entry.ColorIndex;
        if (entry.PlayerId == cSelf)
            continue;
        PlayerOnMap player{};
        player.Position.x = entry.Position.x;
        player.Position.y = entry.Position.y;
        player.Position.z = entry.Position.z;
        player.WorldSpaceId = modSystem.GetGameId(entry.WorldSpaceId);
        player.Heading = entry.Heading;
        player.Inside = entry.Inside;
        if (player.WorldSpaceId)
            players[entry.PlayerId] = player;
    }
    m_players = std::move(players);
}

void MapService::OnDraw() noexcept
{
    if ((m_markers.empty() && m_players.empty()) || !PlayerCharacter::Get())
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
    const float cCenterX = cWidth * 0.5f;
    const float cCompassY = cHeight * kCompassCenterY;
    const float cHalfWidth = cWidth * kCompassHalfWidth;
    // The size of the game's own compass icons.
    const float cIconHeight = cHeight * 0.029f;
    ImDrawList* pDraw = ImGui::GetBackgroundDrawList();

    // The compass follows the camera's heading.
    NiCamera* pNiCamera = pCamera->GetNiCamera();
    if (!pNiCamera)
        return;
    const float cHeading = CameraHeading(pNiCamera);

    // Where a world position sits on the compass (x on screen), and how far it is in metres; false if outside it.
    auto onCompass = [&](const NiPoint3& acPosition, float& aX, float& aDelta, int& aMeters) {
        const float cDx = acPosition.x - pPlayer->position.x;
        const float cDy = acPosition.y - pPlayer->position.y;
        float delta = (std::atan2(cDx, cDy) - cHeading) * 180.f / 3.14159265f;
        while (delta > 180.f)
            delta -= 360.f;
        while (delta < -180.f)
            delta += 360.f;
        if (std::abs(delta) > kCompassHalfAngle)
            return false;
        aX = cCenterX + delta / kCompassHalfAngle * cHalfWidth;
        aDelta = delta;
        aMeters = static_cast<int>(std::sqrt(cDx * cDx + cDy * cDy) / 70.f);
        return true;
    };

    // Other players: their actor where it's loaded nearby (moves smoothly), else what the server last said.
    TiltedPhoques::Map<uint32_t, NiPoint3> livePositions;
    for (auto entity : m_world.view<PlayerComponent, FormIdComponent>())
    {
        const uint32_t cId = m_world.get<PlayerComponent>(entity).Id;
        if (Actor* pActor = Cast<Actor>(TESForm::GetById(m_world.get<FormIdComponent>(entity).Id)))
            livePositions[cId] = pActor->position;
    }
    for (const auto& [playerId, player] : m_players)
    {
        if (player.WorldSpaceId != cWorldSpaceId)
            continue;
        NiPoint3 position = player.Position;
        if (!player.Inside)
        {
            if (auto itor = livePositions.find(playerId); itor != livePositions.end())
                position = itor->second;
        }
        float x = 0.f, delta = 0.f;
        int meters = 0;
        if (!onCompass(position, x, delta, meters))
            continue;
        const float cAlpha = player.Inside ? 0.6f : 1.f;
        DrawPlayerDot(pDraw, {x, cCompassY}, cIconHeight * 0.26f, ColorOf(playerId), cAlpha);
        // Name and distance above the compass while you face them (marker labels go below it).
        if (std::abs(delta) < 12.f)
        {
            const std::string cText = player.Inside ? fmt::format("{} (inside)  \xC2\xB7  {} m", NameOf(playerId).c_str(), meters)
                                                    : fmt::format("{}  \xC2\xB7  {} m", NameOf(playerId).c_str(), meters);
            DrawSkyrimText(pDraw, {x, cCompassY - cIconHeight * 0.7f}, Anchor::Above, cText, cHeight * 0.022f, kTextColor, cAlpha);
        }
    }

    for (const auto& [playerId, marker] : m_markers)
    {
        if (marker.WorldSpaceId != cWorldSpaceId)
            continue;
        float x = 0.f, delta = 0.f;
        int meters = 0;
        if (!onCompass(marker.Position, x, delta, meters))
            continue;

        // Centred on the compass bar, like the game's icons.
        DrawKite(pDraw, {x, cCompassY + cIconHeight * 0.5f}, cIconHeight, ColorOf(playerId));
        // Whose it is and how far, under the compass, while it's roughly straight ahead.
        if (std::abs(delta) < 12.f)
            DrawSkyrimText(pDraw, {x, cCompassY + cIconHeight * 0.75f}, Anchor::Below, fmt::format("{}'s Marker  \xC2\xB7  {} m", NameOf(playerId).c_str(), meters), cHeight * 0.024f);
    }
}

void MapService::DrawOnMap() noexcept
{
    auto* pWindow = BSGraphics::GetMainWindow();
    if (!pWindow)
        return;
    const float cWidth = static_cast<float>(pWindow->uiWindowWidth);
    const float cHeight = static_cast<float>(pWindow->uiWindowHeight);
    const uint32_t cWorldSpaceId = CurrentWorldSpaceId();
    ImDrawList* pDraw = ImGui::GetBackgroundDrawList();
    // The world map is drawn through the main camera, moved above the map.
    NiCamera* pMapCamera = MainCamera();
    if (!pMapCamera)
        return;

    // World position to screen pixels; false if it's off the screen.
    auto toScreen = [&](const NiPoint3& acPosition, ImVec2& aScreen) {
        NiPoint3 screen{};
        if (!ProjectWithCamera(pMapCamera, acPosition, screen) || screen.z < 0.f)
            return false;
        aScreen = {cWidth * screen.x, cHeight * (1.f - screen.y)};
        return screen.x >= 0.f && screen.x <= 1.f && screen.y >= 0.f && screen.y <= 1.f;
    };
    // The world map shows the worldspace the player is in (or the one outside, when inside).
    auto shown = [&](uint32_t aWorldSpaceId) { return !cWorldSpaceId || aWorldSpaceId == cWorldSpaceId; };

    // Other players first, so markers sit on top of them.
    for (const auto& [playerId, player] : m_players)
    {
        ImVec2 center{};
        if (!shown(player.WorldSpaceId) || !toScreen(player.Position, center))
            continue;

        // Which way they face, on screen: project a point a little ahead of them.
        NiPoint3 ahead = player.Position;
        ahead.x += std::sin(player.Heading) * 2000.f;
        ahead.y += std::cos(player.Heading) * 2000.f;
        ImVec2 aheadScreen{};
        toScreen(ahead, aheadScreen);
        ImVec2 direction{aheadScreen.x - center.x, aheadScreen.y - center.y};
        const float cLength = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        direction = cLength > 0.5f ? ImVec2{direction.x / cLength, direction.y / cLength} : ImVec2{0.f, -1.f};

        const float cAlpha = player.Inside ? 0.6f : 1.f;
        const float cSize = cHeight * 0.0165f;
        DrawPlayerArrow(pDraw, center, direction, cSize, ColorOf(playerId), cAlpha);
        const std::string cName = player.Inside ? fmt::format("{} (inside)", NameOf(playerId).c_str()) : std::string(NameOf(playerId).c_str());
        DrawSkyrimText(pDraw, {center.x, center.y + cSize * 1.2f}, Anchor::Below, cName, cHeight * 0.024f, kTextColor, cAlpha);
    }

    for (const auto& [playerId, marker] : m_markers)
    {
        ImVec2 tip{};
        if (!shown(marker.WorldSpaceId) || !toScreen(marker.Position, tip))
            continue;

        const float cIconHeight = cHeight * 0.034f;
        DrawKite(pDraw, tip, cIconHeight, ColorOf(playerId));
        DrawSkyrimText(pDraw, {tip.x, tip.y - cIconHeight * 1.12f}, Anchor::Above, fmt::format("{}'s Marker", NameOf(playerId).c_str()), cHeight * 0.026f);
    }
}
