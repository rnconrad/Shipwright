#include "SohImGuiImpl.h"

#include <iostream>
#include <iomanip>
#include <map>
#include <utility>
#include <regex>

#include "Archive.h"
#include "Environment.h"
#include "GameSettings.h"
#include "SohConsole.h"
#include "SohHooks.h"
#include "Lib/ImGui/imgui_internal.h"
#include "GlobalCtx2.h"
#include "ResourceMgr.h"
#include "TextureMod.h"
#include "Window.h"
#include "Cvar.h"
#include "Texture.h"
#include "../Fast3D/gfx_pc.h"
#include "Lib/stb/stb_image.h"
#include "Lib/Fast3D/gfx_rendering_api.h"
#include "Lib/spdlog/include/spdlog/common.h"
#include "Utils/StringHelper.h"

#ifdef ENABLE_OPENGL
#include "Lib/ImGui/backends/imgui_impl_opengl3.h"
#include "Lib/ImGui/backends/imgui_impl_sdl.h"

#endif

#if defined(ENABLE_DX11) || defined(ENABLE_DX12)
#include "Lib/ImGui/backends/imgui_impl_dx11.h"
#include "Lib/ImGui/backends/imgui_impl_win32.h"
#include <Windows.h>

IMGUI_IMPL_API LRESULT  ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif

using namespace Ship;
bool oldCursorState = true;

#define TOGGLE_BTN ImGuiKey_F1
#define TTS_TOGGLE_BTN ImGuiKey_F2
#define FOCUS_MENU_BTN ImGuiKey_F3
#define FOCUS_CONSOLE_BTN ImGuiKey_F4
#define HOOK(b) if(b) needs_save = true;
OSContPad* pads;

std::map<std::string, GameAsset*> DefaultAssets;

namespace SohImGui {

    WindowImpl impl;
    ImGuiIO* io;
    Console* console = new Console;
    bool p_open = false;
    bool needs_save = false;
    bool is_hovering = false;
    ImGuiID last_hovered_id;
    ImGuiID first_id, last_id;
    std::map<ImGuiID, std::string> tts_map = {};
    std::regex num_reg("([0-9]+ %)");

    std::map<std::string, std::vector<std::string>> windowCategories;
    std::map<std::string, CustomWindow> customWindows;

    void ImGuiWMInit() {
        switch (impl.backend) {
        case Backend::SDL:
            ImGui_ImplSDL2_InitForOpenGL(static_cast<SDL_Window*>(impl.sdl.window), impl.sdl.context);
            break;
        case Backend::DX11:
            ImGui_ImplWin32_Init(impl.dx11.window);
            break;
        }
    }

    void ImGuiBackendInit() {
        switch (impl.backend) {
        case Backend::SDL:
            ImGui_ImplOpenGL3_Init("#version 120");
            break;
        case Backend::DX11:
            ImGui_ImplDX11_Init(static_cast<ID3D11Device*>(impl.dx11.device), static_cast<ID3D11DeviceContext*>(impl.dx11.device_context));
            break;
        }
    }

    void ImGuiProcessEvent(EventImpl event) {
        switch (impl.backend) {
        case Backend::SDL:
            ImGui_ImplSDL2_ProcessEvent(static_cast<const SDL_Event*>(event.sdl.event));
            break;
        case Backend::DX11:
            ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(event.win32.handle), event.win32.msg, event.win32.wparam, event.win32.lparam);
            break;
        }
    }

    void ImGuiWMNewFrame() {
        switch (impl.backend) {
        case Backend::SDL:
            ImGui_ImplSDL2_NewFrame(static_cast<SDL_Window*>(impl.sdl.window));
            break;
        case Backend::DX11:
            ImGui_ImplWin32_NewFrame();
            break;
        }
    }

    void ImGuiBackendNewFrame() {
        switch (impl.backend) {
        case Backend::SDL:
            ImGui_ImplOpenGL3_NewFrame();
            break;
        case Backend::DX11:
            ImGui_ImplDX11_NewFrame();
            break;
        }
    }

    void ImGuiRenderDrawData(ImDrawData* data) {
        switch (impl.backend) {
        case Backend::SDL:
            ImGui_ImplOpenGL3_RenderDrawData(data);
            break;
        case Backend::DX11:
            ImGui_ImplDX11_RenderDrawData(data);
            break;
        }
    }

    bool UseViewports() {
        switch (impl.backend) {
        case Backend::DX11:
            return true;
        }
        return false;
    }

    void SohImGui::ReadText(std::string text, bool force = false) {
        if (Game::Settings.accessibility.enable_tts || force)
            GlobalCtx2::GetInstance()->GetWindow()->ReadText(text.c_str());
    }

    void SohImGui::ShowCursor(bool hide, Dialogues d) {
        if (d == Dialogues::dLoadSettings) {
            GlobalCtx2::GetInstance()->GetWindow()->ShowCursor(hide);
            return;
        }

        if (d == Dialogues::dConsole && Game::Settings.debug.menu_bar) {
            return;
        }
        if (!GlobalCtx2::GetInstance()->GetWindow()->IsFullscreen()) {
            oldCursorState = false;
            return;
        }

        if (oldCursorState != hide) {
            oldCursorState = hide;
            GlobalCtx2::GetInstance()->GetWindow()->ShowCursor(hide);
        }
    }

    // first_or_last_item is 0 for first, 1 for last, -1 for all others
    bool SohImGui::BeginMenu(const char *label, bool enabled = true, int first_or_last_item = -1) {
        ImGuiID id = ImGui::GetID(label);
        switch (first_or_last_item) {
        case 0:
            first_id = id;
            break;
        case 1:
            last_id = id;
            break;
        }
        tts_map[id] = label;
        return ImGui::BeginMenu(label, enabled);
    }

    bool SohImGui::Checkbox(const char *label, bool* v) {
        tts_map[ImGui::GetID(label)] = StringHelper::Sprintf("%s, %s", label, (*v ? "enabled" : "disabled"));
        return ImGui::Checkbox(label, v);
    }

    bool SohImGui::MenuItem(const char* label, const char* shortcut, bool* p_selected, bool enabled = true) {
        tts_map[ImGui::GetID(label)] = label;
        return ImGui::MenuItem(label, shortcut, p_selected, enabled);
    }

    bool SohImGui::SliderFloat(const char* label, const char* str_id, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags = 0) {
        tts_map[ImGui::GetID(str_id)] = label;
        ImGui::Text(label);
        return ImGui::SliderFloat(str_id, v, v_min, v_max, format, flags);
    }

    void LoadTexture(const std::string& name, const std::string& path) {
        GfxRenderingAPI* api = gfx_get_current_rendering_api();
        const auto res = GlobalCtx2::GetInstance()->GetResourceManager()->LoadFile(normalize(path));

        const auto asset = new GameAsset{ api->new_texture() };
        uint8_t* img_data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(res->buffer.get()), res->dwBufferSize, &asset->width, &asset->height, nullptr, 4);

        if (img_data == nullptr) {
            std::cout << "Found error: " << stbi_failure_reason() << std::endl;
            return;
        }

        api->select_texture(0, asset->textureId);
        api->set_sampler_parameters(0, false, 0, 0);
        api->upload_texture(img_data, asset->width, asset->height);

        DefaultAssets[name] = asset;
        stbi_image_free(img_data);
    }

    void LoadResource(const std::string& name, const std::string& path, const ImVec4& tint) {
        GfxRenderingAPI* api = gfx_get_current_rendering_api();
        const auto res = static_cast<Ship::Texture*>(GlobalCtx2::GetInstance()->GetResourceManager()->LoadResource(normalize(path)).get());

        std::vector<uint8_t> texBuffer;
        texBuffer.reserve(res->width * res->height * 4);

        switch (res->texType) {
        case Ship::TextureType::RGBA32bpp:
            texBuffer.assign(res->imageData, res->imageData + (res->width * res->height * 4));
            break;
        case Ship::TextureType::GrayscaleAlpha8bpp:
            for (int32_t i = 0; i < res->width * res->height; i++) {
                uint8_t ia = res->imageData[i];
                uint8_t color = ((ia >> 4) & 0xF) * 255 / 15;
                uint8_t alpha = (ia & 0xF) * 255 / 15;
                texBuffer.push_back(color);
                texBuffer.push_back(color);
                texBuffer.push_back(color);
                texBuffer.push_back(alpha);
            }
            break;
        default:
            // TODO convert other image types
            SPDLOG_WARN("SohImGui::LoadResource: Attempting to load unsupporting image type %s", path.c_str());
            return;
        }

        for (size_t pixel = 0; pixel < texBuffer.size() / 4; pixel++) {
            texBuffer[pixel * 4 + 0] *= tint.x;
            texBuffer[pixel * 4 + 1] *= tint.y;
            texBuffer[pixel * 4 + 2] *= tint.z;
            texBuffer[pixel * 4 + 3] *= tint.w;
        }

        const auto asset = new GameAsset{ api->new_texture() };

        api->select_texture(0, asset->textureId);
        api->set_sampler_parameters(0, false, 0, 0);
        api->upload_texture(texBuffer.data(), res->width, res->height);

        DefaultAssets[name] = asset;
    }

    void Init(WindowImpl window_impl) {
        impl = window_impl;
        Game::LoadSettings();
        ImGuiContext* ctx = ImGui::CreateContext();
        ImGui::SetCurrentContext(ctx);
        io = &ImGui::GetIO();
        io->ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
        if (UseViewports()) {
            io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        }
        console->Init();
        ImGuiWMInit();
        ImGuiBackendInit();

        ModInternal::registerHookListener({ GFX_INIT, [](const HookEvent ev) {

            if (GlobalCtx2::GetInstance()->GetWindow()->IsFullscreen())
                ShowCursor(Game::Settings.debug.menu_bar, Dialogues::dLoadSettings);

            LoadTexture("Game_Icon", "assets/ship_of_harkinian/icons/gSohIcon.png");
            LoadTexture("A-Btn", "assets/ship_of_harkinian/buttons/ABtn.png");
            LoadTexture("B-Btn", "assets/ship_of_harkinian/buttons/BBtn.png");
            LoadTexture("L-Btn", "assets/ship_of_harkinian/buttons/LBtn.png");
            LoadTexture("R-Btn", "assets/ship_of_harkinian/buttons/RBtn.png");
            LoadTexture("Z-Btn", "assets/ship_of_harkinian/buttons/ZBtn.png");
            LoadTexture("Start-Btn", "assets/ship_of_harkinian/buttons/StartBtn.png");
            LoadTexture("C-Left", "assets/ship_of_harkinian/buttons/CLeft.png");
            LoadTexture("C-Right", "assets/ship_of_harkinian/buttons/CRight.png");
            LoadTexture("C-Up", "assets/ship_of_harkinian/buttons/CUp.png");
            LoadTexture("C-Down", "assets/ship_of_harkinian/buttons/CDown.png");
        } });

        ModInternal::registerHookListener({ CONTROLLER_READ, [](const HookEvent ev) {
            pads = static_cast<OSContPad*>(ev->baseArgs["cont_pad"]);
        } });
        Game::InitSettings();
    }

    void Update(EventImpl event) {
        if (needs_save) {
            Game::SaveSettings();
            needs_save = false;
        }
        ImGuiProcessEvent(event);
    }

    void FocusConsole() {
        // Focusing the CMDInput control should just work but it doesn't... so we gotta hack it a bit.
        // Focus Console window and fake a x2 Tab input
        ImGui::FocusWindow(ImGui::FindWindowByName("Console"));
        io->AddKeyEvent(ImGuiKey_Tab, true);
        io->AddKeyEvent(ImGuiKey_Tab, false);
        io->AddKeyEvent(ImGuiKey_Tab, true);
        io->AddKeyEvent(ImGuiKey_Tab, false);
        ReadText("Console focused");
    }

#define BindButton(btn, status) ImGui::Image(GetTextureByID(DefaultAssets[btn]->textureId), ImVec2(16.0f * scale, 16.0f * scale), ImVec2(0, 0), ImVec2(1.0f, 1.0f), ImVec4(255, 255, 255, (status) ? 255 : 0));

    void BindAudioSlider(const char* name, const char* key, float* value, SeqPlayers playerId) {
        int v = static_cast<int>(100 * *(value));
        std::string label = StringHelper::Sprintf(name, v);
        tts_map[ImGui::GetID((std::string("##") + key).c_str())] = label;
        ImGui::Text(label.c_str());
        if (ImGui::SliderFloat((std::string("##") + key).c_str(), value, 0.0f, 1.0f, "")) {
            const float volume = floorf(*(value) * 100) / 100;
            CVar_SetFloat(key, volume);
            needs_save = true;
            Game::SetSeqPlayerVolume(playerId, volume);
        }
    }

    extern "C" uint8_t __enableGameInput;

    void DrawMainMenuAndCalculateGameSize() {
        console->Update();
        ImGuiBackendNewFrame();
        ImGuiWMNewFrame();
        ImGui::NewFrame();
        __enableGameInput = true;

        const std::shared_ptr<Window> wnd = GlobalCtx2::GetInstance()->GetWindow();
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoResize;
        if (Game::Settings.debug.menu_bar) window_flags |= ImGuiWindowFlags_MenuBar;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(wnd->GetCurrentWidth(), wnd->GetCurrentHeight()));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
        ImGui::Begin("Main - Deck", nullptr, window_flags);
        ImGui::PopStyleVar(3);

        ImVec2 top_left_pos = ImGui::GetWindowPos();

        const ImGuiID dockId = ImGui::GetID("main_dock");

        if (!ImGui::DockBuilderGetNode(dockId)) {
            ImGui::DockBuilderRemoveNode(dockId);
            ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_NoTabBar);

            ImGui::DockBuilderDockWindow("OoT Master Quest", dockId);

            ImGui::DockBuilderFinish(dockId);
        }

        ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        if (ImGui::IsKeyPressed(TOGGLE_BTN)) {
            Game::Settings.debug.menu_bar = !Game::Settings.debug.menu_bar;
            needs_save = true;
            GlobalCtx2::GetInstance()->GetWindow()->dwMenubar = Game::Settings.debug.menu_bar;
            ShowCursor(Game::Settings.debug.menu_bar, Dialogues::dMenubar);
            ReadText(StringHelper::Sprintf("Debug Menu %s", (Game::Settings.debug.menu_bar ? "enabled" : "disabled")));
        }

        if (ImGui::IsKeyPressed(TTS_TOGGLE_BTN)) {
            Game::Settings.accessibility.enable_tts = !Game::Settings.accessibility.enable_tts;
            needs_save = true;
            ReadText(StringHelper::Sprintf("Text-To-Speech %s", (Game::Settings.accessibility.enable_tts ? "enabled" : "disabled")), true);
        }

        // Keyboard Navigation for Accessibility
        if (Game::Settings.debug.menu_bar) {
            if (ImGui::IsKeyPressed(FOCUS_MENU_BTN)) {
                ImGuiContext* current_ctx = ImGui::GetCurrentContext();
                current_ctx->NavDisableHighlight = false;

                if (ImGui::IsAnyItemFocused()) {
                    ImGuiID focused = ImGui::GetFocusID();

                    if (console->opened) {
                        // Get an ID for our Console's input box
                        std::string cmd_input = "CMDInput";
                        ImGuiWindow* console_window = ImGui::FindWindowByName("Console");
                        ImGuiID cmd_input_id = ImGui::GetIDWithSeed(cmd_input.c_str(), cmd_input.c_str() + cmd_input.size(), console_window->ID);

                        if (focused == cmd_input_id) {
                            ImGui::FocusWindow(NULL);
                            ReadText("Game focused");
                        }
                        else {
                            FocusConsole();
                        }
                    }
                    else {
                        ImGui::FocusWindow(NULL);
                        ReadText("Game focused");
                    }
                }
                else {
                    // Dirty... Fake an input to our first menu item
                    io->AddKeyEvent(ImGuiKey_Escape, true);
                    io->AddKeyEvent(ImGuiKey_Escape, false);
                    ImGui::SetFocusID(first_id, current_ctx->CurrentWindow);
                    ImGui::SetHoveredID(first_id);
                    io->AddKeyEvent(ImGuiKey_LeftArrow, true);
                    io->AddKeyEvent(ImGuiKey_LeftArrow, false);
                    ReadText("Menu focused");
                }
            }

            if (ImGui::IsKeyReleased(ImGuiKey_LeftArrow) || ImGui::IsKeyReleased(ImGuiKey_RightArrow)
                || ImGui::IsKeyReleased(ImGuiKey_DownArrow) || ImGui::IsKeyReleased(ImGuiKey_UpArrow)) {
                if (ImGui::IsAnyItemFocused()) {
                    ImGuiID focused = ImGui::GetFocusID();
                    

                    if (ImGui::IsAnyItemActive()) {
                        // We are focused inside a slider in edit mode, just read back the value please
                        std::cmatch slider_val;
                        regex_search(tts_map[focused].c_str(), slider_val, num_reg);
                        ReadText(slider_val.str());
                    }
                    else {
                        ReadText(tts_map[focused]);
                    }
                }
            }

            if (ImGui::IsKeyReleased(ImGuiKey_Space)) {
                if (ImGui::IsAnyItemActive()) {
                    std::string cmd_input = "CMDInput";
                    ImGuiWindow* console_window = ImGui::FindWindowByName("Console");
                    ImGuiID cmd_input_id = ImGui::GetIDWithSeed(cmd_input.c_str(), cmd_input.c_str() + cmd_input.size(), console_window->ID);
                    ImGuiID active = ImGui::GetActiveID();
                    if (active != cmd_input_id) {
                        if (ImGui::IsAnyItemFocused()) {
                            ReadText(StringHelper::Sprintf("selected %s", tts_map[active].c_str()));
                        }
                        else
                            ReadText(StringHelper::Sprintf("deselected %s", tts_map[active].c_str()));
                    }
                }
            }

            if (ImGui::IsAnyItemHovered()) {
                ImGuiID hovered = ImGui::GetHoveredID();
                if (tts_map.count(hovered) > 0 &&
                    (!is_hovering || hovered != last_hovered_id)) {
                    ReadText(tts_map[hovered]);
                    is_hovering = true;
                    last_hovered_id = hovered;
                }
            }
            else {
                is_hovering = false;
            }
            ///////////
        }

        if (ImGui::IsKeyReleased(FOCUS_CONSOLE_BTN) && console->opened) {
            FocusConsole();
        }

        if (ImGui::BeginMenuBar()) {
            if (DefaultAssets.contains("Game_Icon")) {
                ImGui::SetCursorPos(ImVec2(5, 2.5f));
                ImGui::Image(GetTextureByID(DefaultAssets["Game_Icon"]->textureId), ImVec2(16.0f, 16.0f));
                ImGui::SameLine();
                ImGui::SetCursorPos(ImVec2(25, 0));
            }
            ImGui::Text("Shipwright");
            ImGui::Separator();

            if (SohImGui::BeginMenu("Accessibility", true, 0)) {
                if (SohImGui::Checkbox("Text-To-Speech", &Game::Settings.accessibility.enable_tts)) {
                    CVar_SetS32(const_cast<char*>("gMessageTTS"), Game::Settings.accessibility.enable_tts);
                    ReadText(StringHelper::Sprintf("Text-To-Speech %s", (Game::Settings.accessibility.enable_tts ? "enabled" : "disabled")), true);
                    needs_save = true;
                }

                ImGui::EndMenu();
            }

            if (SohImGui::BeginMenu("Audio")) {
                ImGui::SetItemDefaultFocus();
                const float volume = Game::Settings.audio.master;
                if (SohImGui::SliderFloat(StringHelper::Sprintf("Master Volume: %d %%", static_cast<int>(100 * volume)).c_str(), "##Master_Vol", &Game::Settings.audio.master, 0.0f, 1.0f, "")) {
                    CVar_SetFloat("gGameMasterVolume", volume);
                    needs_save = true;
                }

                BindAudioSlider("Main Music Volume: %d %%", "gMainMusicVolume", &Game::Settings.audio.music_main, SEQ_BGM_MAIN);
                BindAudioSlider("Sub Music Volume: %d %%", "gSubMusicVolume", &Game::Settings.audio.music_sub, SEQ_BGM_SUB);
                BindAudioSlider("Sound Effects Volume: %d %%", "gSFXMusicVolume", &Game::Settings.audio.sfx, SEQ_SFX);
                BindAudioSlider("Fanfare Volume: %d %%", "gFanfareVolume", &Game::Settings.audio.fanfare, SEQ_FANFARE);

                ImGui::EndMenu();
            }

            if (SohImGui::BeginMenu("Controller")) {
                if (SohImGui::SliderFloat(StringHelper::Sprintf("Gyro Sensitivity: %d %%", static_cast<int>(100 * Game::Settings.controller.gyro_sensitivity)).c_str(), "##GYROSCOPE", &Game::Settings.controller.gyro_sensitivity, 0.0f, 1.0f, "")) {
                    needs_save = true;
                }

                if (ImGui::Button("Recalibrate Gyro")) {
                    Game::Settings.controller.gyroDriftX = 0;
                    Game::Settings.controller.gyroDriftY = 0;
                }

                ImGui::Separator();

                if (SohImGui::SliderFloat(StringHelper::Sprintf("Rumble Strength: %d %%", static_cast<int>(100 * Game::Settings.controller.rumble_strength)).c_str(), "##RUMBLE", &Game::Settings.controller.rumble_strength, 0.0f, 1.0f, "")) {
                    needs_save = true;
                }

                if (SohImGui::Checkbox("Show Inputs", &Game::Settings.controller.input_enabled)) {
                    needs_save = true;
                }

                if (SohImGui::SliderFloat(StringHelper::Sprintf("Input Scale: %.1f", Game::Settings.controller.input_scale).c_str(), "##Input", &Game::Settings.controller.input_scale, 1.0f, 3.0f, "")) {
                    needs_save = true;
                }

                ImGui::Separator();

                if (SohImGui::Checkbox("Dpad Support on Pause and File Select", &Game::Settings.controller.dpad_pause_name)) {
                    CVar_SetS32("gDpadPauseName", Game::Settings.controller.dpad_pause_name);
                    needs_save = true;
                }

                if (SohImGui::Checkbox("DPad Support in Ocarina and Text Choice", &Game::Settings.controller.dpad_ocarina_text)) {
                    CVar_SetS32("gDpadOcarinaText", Game::Settings.controller.dpad_ocarina_text);
                    needs_save = true;
                }

                if (SohImGui::Checkbox("DPad Support for Browsing Shop Items", &Game::Settings.controller.dpad_shop)) {
                    CVar_SetS32("gDpadShop", Game::Settings.controller.dpad_shop);
                    needs_save = true;
                }

                ImGui::EndMenu();
            }

            if (SohImGui::BeginMenu("Enhancements")) {
                ImGui::Text("Gameplay");
                ImGui::Separator();

                if (SohImGui::Checkbox("Fast Text", &Game::Settings.enhancements.fast_text)) {
                    CVar_SetS32("gFastText", Game::Settings.enhancements.fast_text);
                    needs_save = true;
                }

                if (SohImGui::Checkbox("Minimal UI", &Game::Settings.enhancements.minimal_ui)) {
                    CVar_SetS32("gMinimalUI", Game::Settings.enhancements.minimal_ui);
                    needs_save = true;
                }

                if (SohImGui::Checkbox("MM Bunny Hood", &Game::Settings.enhancements.mm_bunny_hood)) {
                    CVar_SetS32("gMMBunnyHood", Game::Settings.enhancements.mm_bunny_hood);
                    needs_save = true;
                }

                ImGui::Text("Graphics");
                ImGui::Separator();

                HOOK(SohImGui::Checkbox("N64 Mode", &Game::Settings.debug.n64mode));

                if (SohImGui::Checkbox("Animated Link in Pause Menu", &Game::Settings.enhancements.animated_pause_menu)) {
                    CVar_SetS32("gPauseLiveLink", Game::Settings.enhancements.animated_pause_menu);
                    needs_save = true;
                }

                if (SohImGui::Checkbox("Disable LOD", &Game::Settings.enhancements.disable_lod)) {
                    CVar_SetS32("gDisableLOD", Game::Settings.enhancements.disable_lod);
                    needs_save = true;
                }

                ImGui::EndMenu();
            }

            if (SohImGui::BeginMenu("Developer Tools")) {
                HOOK(SohImGui::MenuItem("Stats", nullptr, &Game::Settings.debug.soh));
                HOOK(SohImGui::MenuItem("Console", nullptr, &console->opened));

                ImGui::Text("Debug");
                ImGui::Separator();

                if (SohImGui::Checkbox("Debug Mode", &Game::Settings.cheats.debug_mode)) {
                    CVar_SetS32("gDebugEnabled", Game::Settings.cheats.debug_mode);
                    needs_save = true;
                }

                ImGui::EndMenu();
            }

            if (SohImGui::BeginMenu("Graphics")) {
                HOOK(SohImGui::MenuItem("Anti-aliasing", nullptr, &Game::Settings.graphics.show));
                ImGui::EndMenu();
            }

            if (SohImGui::BeginMenu("Cheats")) {
                if (SohImGui::Checkbox("Infinite Money", &Game::Settings.cheats.infinite_money)) {
                    CVar_SetS32("gInfiniteMoney", Game::Settings.cheats.infinite_money);
                    needs_save = true;
                }

                if (SohImGui::Checkbox("Infinite Health", &Game::Settings.cheats.infinite_health)) {
                    CVar_SetS32("gInfiniteHealth", Game::Settings.cheats.infinite_health);
                    needs_save = true;
                }

                if (SohImGui::Checkbox("Infinite Ammo", &Game::Settings.cheats.infinite_ammo)) {
                    CVar_SetS32("gInfiniteAmmo", Game::Settings.cheats.infinite_ammo);
                    needs_save = true;
                }

                if (SohImGui::Checkbox("Infinite Magic", &Game::Settings.cheats.infinite_magic)) {
                    CVar_SetS32("gInfiniteMagic", Game::Settings.cheats.infinite_magic);
                    needs_save = true;
                }

                if (SohImGui::Checkbox("No Clip", &Game::Settings.cheats.no_clip)) {
                    CVar_SetS32("gNoClip", Game::Settings.cheats.no_clip);
                    needs_save = true;
                }

                if (SohImGui::Checkbox("Climb Everything", &Game::Settings.cheats.climb_everything)) {
                    CVar_SetS32("gClimbEverything", Game::Settings.cheats.climb_everything);
                    needs_save = true;
                }

                if (SohImGui::Checkbox("Moon Jump on L", &Game::Settings.cheats.moon_jump_on_l)) {
                    CVar_SetS32("gMoonJumpOnL", Game::Settings.cheats.moon_jump_on_l);
                    needs_save = true;
                }

                if (SohImGui::Checkbox("Super Tunic", &Game::Settings.cheats.super_tunic)) {
                    CVar_SetS32("gSuperTunic", Game::Settings.cheats.super_tunic);
                    needs_save = true;
                }

                ImGui::EndMenu();
            }

            for (const auto& category : windowCategories) {
                if (SohImGui::BeginMenu(category.first.c_str())) {
                    for (const std::string& name : category.second) {
                        HOOK(SohImGui::MenuItem(name.c_str(), nullptr, &customWindows[name].enabled));
                    }
                    ImGui::EndMenu();
                }
            }

            ImGui::EndMenuBar();
        }

        ImGui::End();

        if (Game::Settings.debug.soh) {
            const float framerate = ImGui::GetIO().Framerate;
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGui::Begin("Debug Stats", nullptr, ImGuiWindowFlags_None);

            ImGui::Text("Platform: Windows");
            ImGui::Text("Status: %.3f ms/frame (%.1f FPS)", 1000.0f / framerate, framerate);
            ImGui::End();
            ImGui::PopStyleColor();
        }

        if (Game::Settings.graphics.show) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGui::Begin("Anti-aliasing settings", nullptr, ImGuiWindowFlags_None);
            ImGui::Text("Internal Resolution:");
            ImGui::SliderInt("Mul", reinterpret_cast<int*>(&gfx_current_dimensions.internal_mul), 1, 8);
            ImGui::Text("MSAA:");
            ImGui::SliderInt("MSAA", reinterpret_cast<int*>(&gfx_msaa_level), 1, 8);
            ImGui::End();
            ImGui::PopStyleColor();
        }

        console->Draw();

        if (ImGui::IsAnyItemFocused()) {
            __enableGameInput = false;
        }

        for (auto& windowIter : customWindows) {
            CustomWindow& window = windowIter.second;
            if (window.drawFunc != nullptr) {
                window.drawFunc(window.enabled);
            }
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;
        ImGui::Begin("OoT Master Quest", nullptr, flags);
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();

        ImVec2 main_pos = ImGui::GetWindowPos();
        main_pos.x -= top_left_pos.x;
        main_pos.y -= top_left_pos.y;
        ImVec2 size = ImGui::GetContentRegionAvail();
        ImVec2 pos = ImVec2(0, 0);
        gfx_current_dimensions.width = size.x * gfx_current_dimensions.internal_mul;
        gfx_current_dimensions.height = size.y * gfx_current_dimensions.internal_mul;
        gfx_current_game_window_viewport.x = main_pos.x;
        gfx_current_game_window_viewport.y = main_pos.y;
        gfx_current_game_window_viewport.width = size.x;
        gfx_current_game_window_viewport.height = size.y;
        if (Game::Settings.debug.n64mode) {
            gfx_current_dimensions.width = 320;
            gfx_current_dimensions.height = 240;
            const int sw = size.y * 320 / 240;
            gfx_current_game_window_viewport.x += (size.x - sw) / 2;
            gfx_current_game_window_viewport.width = sw;
            pos = ImVec2(size.x / 2 - sw / 2, 0);
            size = ImVec2(sw, size.y);
        }
    }

    void DrawFramebufferAndGameInput() {
        ImVec2 main_pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetContentRegionAvail();
        ImVec2 pos = ImVec2(0, 0);
        if (Game::Settings.debug.n64mode) {
            const int sw = size.y * 320 / 240;
            pos = ImVec2(size.x / 2 - sw / 2, 0);
            size = ImVec2(sw, size.y);
        }
        std::string fb_str = SohUtils::getEnvironmentVar("framebuffer");
        if (!fb_str.empty()) {
            uintptr_t fbuf = (uintptr_t)std::stoull(fb_str);
            //ImGui::ImageSimple(reinterpret_cast<ImTextureID>(fbuf), pos, size);
            ImGui::SetCursorPos(pos);
            ImGui::Image(reinterpret_cast<ImTextureID>(fbuf), size);
        }

        ImGui::End();

        const float scale = Game::Settings.controller.input_scale;
        ImVec2 BtnPos = ImVec2(160 * scale, 85 * scale);

        if (Game::Settings.controller.input_enabled) {
            ImGui::SetNextWindowSize(BtnPos);
            ImGui::SetNextWindowPos(ImVec2(main_pos.x + size.x - BtnPos.x - 20, main_pos.y + size.y - BtnPos.y - 20));

            if (pads != nullptr && ImGui::Begin("Game Buttons", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground)) {
                ImGui::SetCursorPosY(32 * scale);

                ImGui::BeginGroup();
                const ImVec2 cPos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(cPos.x + 10 * scale, cPos.y - 20 * scale));
                BindButton("L-Btn", pads[0].button & BTN_L);
                ImGui::SetCursorPos(ImVec2(cPos.x + 16 * scale, cPos.y));
                BindButton("C-Up", pads[0].button & BTN_CUP);
                ImGui::SetCursorPos(ImVec2(cPos.x, cPos.y + 16 * scale));
                BindButton("C-Left", pads[0].button & BTN_CLEFT);
                ImGui::SetCursorPos(ImVec2(cPos.x + 32 * scale, cPos.y + 16 * scale));
                BindButton("C-Right", pads[0].button & BTN_CRIGHT);
                ImGui::SetCursorPos(ImVec2(cPos.x + 16 * scale, cPos.y + 32 * scale));
                BindButton("C-Down", pads[0].button & BTN_CDOWN);
                ImGui::EndGroup();

                ImGui::SameLine();

                ImGui::BeginGroup();
                const ImVec2 sPos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(sPos.x + 21, sPos.y - 20 * scale));
                BindButton("Z-Btn", pads[0].button & BTN_Z);
                ImGui::SetCursorPos(ImVec2(sPos.x + 22, sPos.y + 16 * scale));
                BindButton("Start-Btn", pads[0].button & BTN_START);
                ImGui::EndGroup();

                ImGui::SameLine();

                ImGui::BeginGroup();
                const ImVec2 bPos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(bPos.x + 20 * scale, bPos.y - 20 * scale));
                BindButton("R-Btn", pads[0].button & BTN_R);
                ImGui::SetCursorPos(ImVec2(bPos.x + 12 * scale, bPos.y + 8 * scale));
                BindButton("B-Btn", pads[0].button & BTN_B);
                ImGui::SetCursorPos(ImVec2(bPos.x + 28 * scale, bPos.y + 24 * scale));
                BindButton("A-Btn", pads[0].button & BTN_A);
                ImGui::EndGroup();

                ImGui::End();
            }
        }
    }

    void Render() {
        ImGui::Render();
        ImGuiRenderDrawData(ImGui::GetDrawData());
        if (UseViewports()) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    void CancelFrame() {
        ImGui::EndFrame();
        if (UseViewports()) {
            ImGui::UpdatePlatformWindows();
        }
    }

    void BindCmd(const std::string& cmd, CommandEntry entry) {
        console->Commands[cmd] = std::move(entry);
    }

    void AddWindow(const std::string& category, const std::string& name, WindowDrawFunc drawFunc) {
        if (customWindows.contains(name)) {
            SPDLOG_ERROR("SohImGui::AddWindow: Attempting to add duplicate window name %s", name.c_str());
            return;
        }

        customWindows[name] = {
            .enabled = false,
            .drawFunc = drawFunc
        };

        windowCategories[category].emplace_back(name);
    }

    ImTextureID GetTextureByName(const std::string& name) {
        return GetTextureByID(DefaultAssets[name]->textureId);
    }
}
