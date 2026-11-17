#pragma once

#include <imgui/ImGuiDriver.h>

struct RenderSystemD3D9;
struct RenderSystemD3D11;
struct ImFont;

/**
 * @brief Draws the ImGui UI.
 */
struct ImguiService
{
    using TCallback = void();

    ImguiService();
    ~ImguiService() noexcept;

    TP_NOCOPYMOVE(ImguiService);

    void Create(RenderSystemD3D11* apRenderSystem, HWND aHwnd);

    void Render() const;
    void Reset() const;

    LRESULT WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void RawInputHandler(RAWINPUT& aRawinput);

    entt::sink<entt::sigh<TCallback>> OnDraw;

    // Futura Condensed, the font of Skyrim's own menus (from the overlay's assets), for text drawn over the game.
    // nullptr if it couldn't be loaded; draw with the default font then.
    [[nodiscard]] ImFont* GetThemeFont() const noexcept { return m_pThemeFont; }

private:
    ImGuiImpl::ImGuiDriver m_imDriver;
    entt::sigh<TCallback> m_drawSignal;
    ImFont* m_pThemeFont{};
};
