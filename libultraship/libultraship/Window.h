#pragma once
#include <memory>
#include "PR/ultra64/gbi.h"
#include "Lib/Fast3D/gfx_pc.h"
#include "UltraController.h"
#include "Controller.h"
#include "GlobalCtx2.h"

namespace Ship {
	class AudioPlayer;

	class Window {
		public:
			static std::map<size_t, std::vector<std::shared_ptr<Controller>>> Controllers;
			static int32_t lastScancode;

			Window(std::shared_ptr<GlobalCtx2> Context);
			~Window();
			void MainLoop(void (*MainFunction)(void));
			void Init();
			void StartFrame();
			void RunCommands(Gfx* Commands, const std::vector<std::unordered_map<Mtx*, MtxF>>& mtx_replacements);
			void SetFrameDivisor(int divisor);
			void GetPixelDepthPrepare(float x, float y);
			uint16_t GetPixelDepth(float x, float y);
			void ToggleFullscreen();
			void SetFullscreen(bool bIsFullscreen);
			void ShowCursor(bool hide);
			void ReadText(const char textToRead[]);
			const std::string& GetAccessibilityText(uint32_t textId, const std::string& arg);
			uint32_t GetFramebufferWidth(int fb);
			uint32_t GetFramebufferHeight(int fb);
			void ReadFramebufferPixels(int fb, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t type, void* data);

			bool IsFullscreen() { return bIsFullscreen; }
			uint32_t GetCurrentWidth();
			uint32_t GetCurrentHeight();
			uint32_t dwMenubar;
			std::shared_ptr<GlobalCtx2> GetContext() { return Context.lock(); }
			std::shared_ptr<AudioPlayer> GetAudioPlayer() { return APlayer; }

		protected:
		private:
			static bool KeyDown(int32_t dwScancode);
			static bool KeyUp(int32_t dwScancode);
			static void AllKeysUp(void);
			static void OnFullscreenChanged(bool bIsNowFullscreen);
			void SetAudioPlayer();

			void LoadAccessibilityText();

			std::weak_ptr<GlobalCtx2> Context;
			std::shared_ptr<AudioPlayer> APlayer;

			GfxWindowManagerAPI* WmApi;
			GfxRenderingAPI* RenderingApi;
			bool bIsFullscreen;
			uint32_t dwWidth;
			uint32_t dwHeight;

			std::unordered_multimap<uint16_t, std::pair<uint32_t, std::string>> accessibilityText;
			std::string accessibilityTextInterpolated;
	};
}

