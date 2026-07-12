#include <SDL2/SDL.h>
#include "player.h"
#include "title_data.h"
#include "window_ui.h"
#include "video_data.h"   // kVideoData[], kVideoDataSize  (generated at build time)

static constexpr int kTargetFPS = 25;
static constexpr int kDefaultW  = 1280;
static constexpr int kDefaultH  = 720;
static constexpr int kMaxWindowVideoH = 720;

int main(int /*argc*/, char* /*argv*/[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Init",
                                 SDL_GetError(), nullptr);
        return 1;
    }

    // ── Open embedded video ───────────────────────────────────────────────
    VideoPlayer player;
    if (!player.open(kVideoData, kVideoDataSize)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Video",
                                 "Failed to open embedded video.", nullptr);
        SDL_Quit();
        return 1;
    }

    int vidW = player.videoWidth();
    int vidH = player.videoHeight();
    if (vidW <= 0 || vidH <= 0) { vidW = kDefaultW; vidH = kDefaultH; }

    int normalW = vidW;
    int normalH = vidH;
    if (normalH > kMaxWindowVideoH) {
        normalW = (normalW * kMaxWindowVideoH) / normalH;
        normalH = kMaxWindowVideoH;
    }

    // Keep even dimensions for YUV-friendly scaling.
    normalW &= ~1;
    normalH &= ~1;
    if (normalW <= 0) normalW = 2;
    if (normalH <= 0) normalH = 2;

    // Small size = 50% of the normal displayed size.
    int smallW = (normalW / 2) & ~1;
    int smallH = (normalH / 2) & ~1;
    if (smallW <= 0) smallW = 2;
    if (smallH <= 0) smallH = 2;

    // ── Create borderless window ──────────────────────────────────────────
    SDL_Window* window = SDL_CreateWindow(
        kWindowTitle,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        normalW + 2 * kWindowBorderThickness,
        normalH + kTitleBarHeight + kStatusBarHeight + 2 * kWindowBorderThickness,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Window",
                                 SDL_GetError(), nullptr);
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // ── UI ────────────────────────────────────────────────────────────────
    WindowUI ui;
    ui.init(window, normalW, normalH, kWindowTitle);

    SDL_Texture* videoTexture = nullptr;
    bool running = true;

    // Track last applied UI state to detect changes
    bool lastCollapsed = false;
    bool lastSmall     = false;

    // Current render dimensions (updated when state changes)
    int curW = normalW, curH = normalH;
    SDL_Rect videoRect = {
        kWindowBorderThickness,
        kWindowBorderThickness + kTitleBarHeight,
        curW,
        curH
    };

    const Uint32 frameMs = 1000u / kTargetFPS;

    while (running) {
        Uint32 frameStart = SDL_GetTicks();

        // ── Events ────────────────────────────────────────────────────────
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = false; break; }
            if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER || e.key.keysym.sym == SDLK_q))
            {
                running = false; break;
            }
            ui.handleEvent(e, winW);
        }
        if (!running) break;

        // ── React to UI state changes ─────────────────────────────────────
        bool collapsed = ui.isCollapsed();
        bool small     = ui.isSmall();

        if (collapsed != lastCollapsed || small != lastSmall) {
            curW = small ? smallW : normalW;
            curH = small ? smallH : normalH;
            int newWinW = curW + 2 * kWindowBorderThickness;
            int newWinH = collapsed
                ? kTitleBarHeight + kStatusBarHeight + 2 * kWindowBorderThickness
                : curH + kTitleBarHeight + kStatusBarHeight + 2 * kWindowBorderThickness;
            SDL_SetWindowSize(window, newWinW, newWinH);
            videoRect = {
                kWindowBorderThickness,
                kWindowBorderThickness + kTitleBarHeight,
                curW,
                curH
            };
            // Recreate texture at new size on next decode
            if (videoTexture) { SDL_DestroyTexture(videoTexture); videoTexture = nullptr; }
            winW = newWinW;
            winH = newWinH;
            lastCollapsed = collapsed;
            lastSmall     = small;
        }

        // ── Decode / audio pump ────────────────────────────────────────────
        if (collapsed) {
            if (!player.pumpAudio()) break;
        } else {
            if (!player.nextFrame(renderer, videoTexture)) break;
        }

        // ── Render ───────────────────────────────────────────────────────
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (!collapsed && videoTexture)
            SDL_RenderCopy(renderer, videoTexture, nullptr, &videoRect);

        ui.draw(renderer, winW, winH);
        SDL_RenderPresent(renderer);

        // ── Frame cap ────────────────────────────────────────────────────
        Uint32 elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < frameMs) SDL_Delay(frameMs - elapsed);
    }

    if (videoTexture) SDL_DestroyTexture(videoTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
