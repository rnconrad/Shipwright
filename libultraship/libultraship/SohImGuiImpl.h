#pragma once

#include "Lib/ImGui/imgui.h"
#include "SohConsole.h"

struct GameAsset {
    uint32_t textureId;
    int width;
    int height;
};

namespace SohImGui {
    enum class Backend {
        DX11,
        SDL
    };

    enum class Dialogues {
        dConsole,
        dMenubar,
        dLoadSettings,
    };

    typedef struct {
        Backend backend;
        union {
            struct {
                void* window;
                void* device_context;
                void* device;
            } dx11;
            struct {
                void* window;
                void* context;
            } sdl;
        };
    } WindowImpl;

    typedef union {
        struct {
            void* handle;
            int msg;
            int wparam;
            int lparam;
        } win32;
        struct {
            void* event;
        } sdl;
    } EventImpl;

    extern WindowImpl impl;

    using WindowDrawFunc = void(*)(bool& enabled);

    typedef struct {
        bool enabled;
        WindowDrawFunc drawFunc;
    } CustomWindow;

    extern Console* console;
    void Init(WindowImpl window_impl);
    void Update(EventImpl event);
    void DrawMainMenuAndCalculateGameSize(void);
    void DrawFramebufferAndGameInput(void);
    void Render(void);
    void CancelFrame(void);
    void ShowCursor(bool hide, Dialogues w);
    void ReadText(std::string text, bool force);
    void BindCmd(const std::string& cmd, CommandEntry entry);
    bool BeginMenu(const char* label, bool enabled, int first_or_last_item);
    bool Checkbox(const char* label, bool* v);
    bool MenuItem(const char* label, const char* shortcut, bool* p_selected, bool enabled);
    bool SliderFloat(const char* label, const char* str_id, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags);
    bool Button(const char* label, const ImVec2& size_arg);
    void AddWindow(const std::string& category, const std::string& name, WindowDrawFunc drawFunc);
    void LoadResource(const std::string& name, const std::string& path, const ImVec4& tint = ImVec4(1, 1, 1, 1));
    ImTextureID GetTextureByID(int id);
    ImTextureID GetTextureByName(const std::string& name);
}
