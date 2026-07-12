#include "window_ui.h"
#include <cstdint>

static SDL_HitTestResult SDLCALL windowHitTest(SDL_Window* window, const SDL_Point* area, void* /*data*/) {
    int winW = 0;
    int winH = 0;
    SDL_GetWindowSize(window, &winW, &winH);
    (void)winH;

    const SDL_Rect close = {
        winW - kWindowBorderThickness - kCloseBtnSize,
        kWindowBorderThickness,
        kCloseBtnSize,
        kTitleBarHeight
    };
    const SDL_Rect collapse = {
        kWindowBorderThickness,
        kWindowBorderThickness,
        kBtnSize,
        kTitleBarHeight
    };
    const SDL_Rect size = {
        kWindowBorderThickness + kBtnSize,
        kWindowBorderThickness,
        kBtnSize,
        kTitleBarHeight
    };
    const auto pointInRect = [](int x, int y, SDL_Rect r) {
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    };

    const bool inTitleBar =
        area->y >= kWindowBorderThickness &&
        area->y < kWindowBorderThickness + kTitleBarHeight;

    const bool onButton =
        pointInRect(area->x, area->y, close) ||
        pointInRect(area->x, area->y, collapse) ||
        pointInRect(area->x, area->y, size);

    if (inTitleBar && !onButton) {
        return SDL_HITTEST_DRAGGABLE;
    }
    return SDL_HITTEST_NORMAL;
}

void WindowUI::init(SDL_Window* win, int /*normalW*/, int /*normalH*/, const char* title) {
    window_ = win;
    title_ = title != nullptr ? title : "";
    SDL_SetWindowHitTest(window_, windowHitTest, nullptr);
}

// ── Helpers ───────────────────────────────────────────────────────────────

static bool pointIn(int x, int y, SDL_Rect r) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static void setColor(SDL_Renderer* r, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

static const uint8_t* glyphRows(char ch) {
    static constexpr uint8_t kSpace[7] = {0, 0, 0, 0, 0, 0, 0};
    static constexpr uint8_t kQuestion[7] = {14, 17, 1, 2, 4, 0, 4};
    static constexpr uint8_t kDot[7] = {0, 0, 0, 0, 0, 12, 12};
    static constexpr uint8_t kDash[7] = {0, 0, 0, 31, 0, 0, 0};
    static constexpr uint8_t kUnderscore[7] = {0, 0, 0, 0, 0, 0, 31};
    static constexpr uint8_t kB[7] = {30, 17, 17, 30, 17, 17, 30};
    static constexpr uint8_t kA[7] = {0, 0, 14, 1, 15, 17, 15};
    static constexpr uint8_t kC[7] = {0, 0, 14, 17, 16, 17, 14};
    static constexpr uint8_t kD[7] = {1, 1, 15, 17, 17, 17, 15};
    static constexpr uint8_t kE[7] = {0, 0, 14, 17, 31, 16, 14};
    static constexpr uint8_t kK[7] = {16, 18, 20, 24, 20, 18, 17};
    static constexpr uint8_t kN[7] = {0, 0, 29, 19, 17, 17, 17};
    static constexpr uint8_t kR[7] = {0, 0, 22, 25, 16, 16, 16};

    switch (ch) {
        case ' ': return kSpace;
        case '.': return kDot;
        case '-': return kDash;
        case '_': return kUnderscore;
        case 'A':
        case 'a': return kA;
        case 'B':
        case 'b': return kB;
        case 'C':
        case 'c': return kC;
        case 'D':
        case 'd': return kD;
        case 'E':
        case 'e': return kE;
        case 'K':
        case 'k': return kK;
        case 'N':
        case 'n': return kN;
        case 'R':
        case 'r': return kR;
        default: return kQuestion;
    }
}

// ── Icon drawing ──────────────────────────────────────────────────────────

// × — close
void WindowUI::drawX(SDL_Renderer* r, SDL_Rect rect) const {
    const int pad = 10;
    // int x1 = rect.x + pad*2,        y1 = rect.y + pad;
    // int x2 = rect.x + rect.w - pad*2, y2 = rect.y + rect.h - pad;
    // for (int d = -1; d <= 1; ++d) {
    //     SDL_RenderDrawLine(r, x1, y1 + d, x2, y2 + d);
    //     SDL_RenderDrawLine(r, x1, y2 + d, x2, y1 + d);
    // }
    SDL_Rect box = {rect.x + pad*2, rect.y + pad/2, rect.w - pad * 4, rect.h - pad};
    SDL_RenderFillRect(r, &box);
}

// ∨ (expanded) or ∧ (collapsed) chevron
void WindowUI::drawCollapseIcon(SDL_Renderer* r, SDL_Rect rect) const {
    const int pad = 9;
    int cx = rect.x + rect.w / 2;
    int xl = rect.x + pad*2,        xr = rect.x + rect.w - pad*2;
    int yt = rect.y + pad,        yb = rect.y + rect.h - pad;

    for (int d = -1; d <= 1; ++d) {
        if (!collapsed_) {
            // ∨ — click to collapse
            SDL_RenderDrawLine(r, xl, yt + d, cx, yb + d);
            SDL_RenderDrawLine(r, cx, yb + d, xr, yt + d);
        } else {
            // ∧ — click to expand
            SDL_RenderDrawLine(r, xl, yb + d, cx, yt + d);
            SDL_RenderDrawLine(r, cx, yt + d, xr, yb + d);
        }
    }
}

// Square outline: large when normal (will shrink), small when already small (will grow)
void WindowUI::drawSizeIcon(SDL_Renderer* r, SDL_Rect rect) const {
    int pad = 4;
    SDL_Rect box = {rect.x + pad*4, rect.y + pad, rect.w - pad * 8, rect.h - pad * 2};
    // SDL_RenderDrawRect(r, &box);
    // Second inner rect when normal to hint "will compress"
    if (!small_) {
        SDL_RenderDrawRect(r, &box);
    } else {
        SDL_RenderFillRect(r, &box);
    }
}

void WindowUI::drawTitle(SDL_Renderer* r) const {
    if (title_.empty()) {
        return;
    }

    constexpr int kGlyphW = 5;
    constexpr int kGlyphH = 7;
    constexpr int kGlyphSpacing = 1;
    constexpr int kLeftPadding = 8;

    int x = kWindowBorderThickness + kBtnSize * 2 + kLeftPadding;
    const int y = kWindowBorderThickness + (kTitleBarHeight - kGlyphH) / 2;

    for (char ch : title_) {
        const uint8_t* rows = glyphRows(ch);
        for (int row = 0; row < kGlyphH; ++row) {
            for (int col = 0; col < kGlyphW; ++col) {
                if ((rows[row] & (1 << (kGlyphW - 1 - col))) == 0) {
                    continue;
                }
                SDL_RenderDrawPoint(r, x + col, y + row);
            }
        }
        x += kGlyphW + kGlyphSpacing;
    }
}

void WindowUI::drawStatusBar(SDL_Renderer* r, int winW, int winH) const {
    SDL_Rect statusBar = {
        kWindowBorderThickness,
        winH - kWindowBorderThickness - kStatusBarHeight,
        winW - 2 * kWindowBorderThickness,
        kStatusBarHeight
    };
    setColor(r, kStatusBarBg);
    SDL_RenderFillRect(r, &statusBar);

    const int chipSize = kStatusBarHeight - 6;
    const int chipY = statusBar.y + (statusBar.h - chipSize) / 2;
    SDL_Rect videoChip = {statusBar.x + 6, chipY, chipSize, chipSize};
    SDL_Rect sizeChip = {videoChip.x + chipSize + 6, chipY, chipSize, chipSize};
    setColor(r, collapsed_ ? kStatusOff : kStatusOn);
    SDL_RenderFillRect(r, &videoChip);

    setColor(r, small_ ? kStatusWarn : kStatusOn);
    SDL_RenderFillRect(r, &sizeChip);

    const Uint32 windowFlags = SDL_GetWindowFlags(window_);
    if ((windowFlags & SDL_WINDOW_INPUT_FOCUS) != 0) {
        constexpr Uint32 kBreathPeriodMs = 1400U;
        const Uint32 phase = SDL_GetTicks() % kBreathPeriodMs;
        const Uint32 halfPeriod = kBreathPeriodMs / 2U;
        const Uint32 ramp = phase < halfPeriod ? phase : (kBreathPeriodMs - phase);
        const Uint8 brightness = static_cast<Uint8>(90U + (165U * ramp) / halfPeriod);

        SDL_Rect activeChip = {
            statusBar.x + statusBar.w - chipSize - 6,
            chipY,
            chipSize,
            chipSize
        };
        SDL_Color activeColor = {
            static_cast<Uint8>((static_cast<unsigned>(kStatusFlash.r) * brightness) / 255U),
            static_cast<Uint8>((static_cast<unsigned>(kStatusFlash.g) * brightness) / 255U),
            static_cast<Uint8>((static_cast<unsigned>(kStatusFlash.b) * brightness) / 255U),
            255
        };
        setColor(r, activeColor);
        SDL_RenderFillRect(r, &activeChip);
    }

    setColor(r, kBorderColor);
    SDL_RenderDrawLine(r, statusBar.x, statusBar.y, statusBar.x + statusBar.w - 1, statusBar.y);
}

void WindowUI::drawBorder(SDL_Renderer* r, int winW, int winH) const {
    setColor(r, kBorderColor);
    for (int i = 0; i < kWindowBorderThickness; ++i) {
        SDL_Rect border = {i, i, winW - i * 2, winH - i * 2};
        SDL_RenderDrawRect(r, &border);
    }
}

// ── Draw ──────────────────────────────────────────────────────────────────

void WindowUI::draw(SDL_Renderer* renderer, int winW, int winH) {
    // Background
    setColor(renderer, kTitleBarBg);
    SDL_Rect bar = {
        kWindowBorderThickness,
        kWindowBorderThickness,
        winW - 2 * kWindowBorderThickness,
        kTitleBarHeight
    };
    SDL_RenderFillRect(renderer, &bar);

    // Left: collapse button
    SDL_Rect colr = collapseRect();
    setColor(renderer, hoverCollapse_ ? kButtonHover : kTitleBarBg);
    SDL_RenderFillRect(renderer, &colr);
    setColor(renderer, kForeground);
    drawCollapseIcon(renderer, colr);

    // Left: size toggle button
    SDL_Rect szr = sizeRect();
    setColor(renderer, hoverSize_ ? kButtonHover : kTitleBarBg);
    SDL_RenderFillRect(renderer, &szr);
    setColor(renderer, kForeground);
    drawSizeIcon(renderer, szr);

    setColor(renderer, kForeground);
    drawTitle(renderer);

    // Right: close button
    SDL_Rect cr = closeRect(winW);
    setColor(renderer, hoverClose_ ? kCloseHoverBg : kTitleBarBg);
    SDL_RenderFillRect(renderer, &cr);
    setColor(renderer, kForeground);
    drawX(renderer, cr);

    drawStatusBar(renderer, winW, winH);
    drawBorder(renderer, winW, winH);
}

// ── Event handling ────────────────────────────────────────────────────────

bool WindowUI::handleEvent(const SDL_Event& e, int winW) {
    SDL_Rect cr   = closeRect(winW);
    SDL_Rect colr = collapseRect();
    SDL_Rect szr  = sizeRect();

    if (e.type == SDL_MOUSEMOTION) {
        int mx = e.motion.x, my = e.motion.y;
        hoverClose_    = pointIn(mx, my, cr);
        hoverCollapse_ = pointIn(mx, my, colr);
        hoverSize_     = pointIn(mx, my, szr);
        return hoverClose_ || hoverCollapse_ || hoverSize_;
    }

    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = e.button.x, my = e.button.y;

        if (pointIn(mx, my, cr))   return true;  // handled on up
        if (pointIn(mx, my, colr)) return true;
        if (pointIn(mx, my, szr))  return true;
        return false;
    }

    if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
        int mx = e.button.x, my = e.button.y;

        if (pointIn(mx, my, cr) && hoverClose_) {
            SDL_Event quit{};
            quit.type = SDL_QUIT;
            SDL_PushEvent(&quit);
            return true;
        }
        if (pointIn(mx, my, colr) && hoverCollapse_) {
            collapsed_ = !collapsed_;
            return true;
        }
        if (pointIn(mx, my, szr) && hoverSize_) {
            small_ = !small_;
            return true;
        }
        return false;
    }

    return false;
}
