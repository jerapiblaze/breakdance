#pragma once
#include <SDL2/SDL.h>
#include <string>

static constexpr int kTitleBarHeight = 16;
static constexpr int kStatusBarHeight = 16;
static constexpr int kWindowBorderThickness = 5;
static constexpr int kBtnSize        = 3*kTitleBarHeight;  // buttons are rectangle
static constexpr int kCloseBtnSize   = 2*kTitleBarHeight;  // buttons are rectangle

static constexpr SDL_Color kTitleBarBg   = {18,  18,  18,  255};
static constexpr SDL_Color kStatusBarBg  = {22,  22,  22,  255};
static constexpr SDL_Color kBorderColor  = {70,  70,  70,  255};
static constexpr SDL_Color kButtonHover  = {55,  55,  55,  255};
static constexpr SDL_Color kCloseHoverBg = {196, 43,  28,  255};
static constexpr SDL_Color kForeground   = {220, 220, 220, 255};
static constexpr SDL_Color kStatusOn     = {255,  255, 255, 255};
static constexpr SDL_Color kStatusOff    = {50,  50, 50, 5};
static constexpr SDL_Color kStatusWarn   = {150,  150, 150, 150};
static constexpr SDL_Color kStatusFlash  = {255,  255, 255, 255};

// Title bar layout (left → right):
//   [▼/▲collapse] [□/▪size] ──── drag area ────  [×close]
//
// Collapse: hides the video; window shrinks to title-bar-only height
// Size:     toggles between normal (default fitted size) and small (50%) size
// Close:    quits (pushes SDL_QUIT) — rightmost
struct WindowUI {
    // Call once after creating the window.
    // normalW / normalH are the default displayed video dimensions
    // (NOT including the title bar).
    void init(SDL_Window* win, int normalW, int normalH, const char* title);

    // Draw the window chrome. Call every frame.
    void draw(SDL_Renderer* renderer, int winW, int winH);

    // Process one SDL event. Returns true if the event was consumed.
    // A close click is signalled by pushing SDL_QUIT into the event queue.
    bool handleEvent(const SDL_Event& e, int winW);

    bool isCollapsed() const { return collapsed_; }
    bool isSmall()     const { return small_;     }

private:
    SDL_Window* window_  = nullptr;

    bool collapsed_ = false;
    bool small_     = false;

    bool hoverClose_    = false;
    bool hoverCollapse_ = false;
    bool hoverSize_     = false;
    std::string title_;

    // Left-side buttons (fixed positions)
    static SDL_Rect collapseRect() {
        return {kWindowBorderThickness, kWindowBorderThickness, kBtnSize, kTitleBarHeight};
    }
    static SDL_Rect sizeRect() {
        return {kWindowBorderThickness + kBtnSize, kWindowBorderThickness, kBtnSize, kTitleBarHeight};
    }
    // Right-side close button (needs window width)
    static SDL_Rect closeRect(int winW) {
        return {winW - kWindowBorderThickness - kCloseBtnSize, kWindowBorderThickness, kCloseBtnSize, kTitleBarHeight};
    }

    void drawX(SDL_Renderer* r, SDL_Rect rect) const;
    void drawCollapseIcon(SDL_Renderer* r, SDL_Rect rect) const;
    void drawSizeIcon(SDL_Renderer* r, SDL_Rect rect) const;
    void drawTitle(SDL_Renderer* r) const;
    void drawStatusBar(SDL_Renderer* r, int winW, int winH) const;
    void drawBorder(SDL_Renderer* r, int winW, int winH) const;
};
