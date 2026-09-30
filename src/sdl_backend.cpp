#include "sdl_backend.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

SdlBackend::~SdlBackend() {
    if (pointer_)
        lv_indev_delete(pointer_);
    if (display_)
        lv_display_delete(display_);
    if (texture_)
        SDL_DestroyTexture(texture_);
    if (renderer_)
        SDL_DestroyRenderer(renderer_);
    if (window_)
        SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool SdlBackend::init(const char* title) {
#ifdef TARGET_WEBOS
    // LG's SDL fork: deliver the Back key to us (scancode 482) instead of
    // letting the system close/hide the launcher.
    SDL_SetHint("SDL_WEBOS_ACCESS_POLICY_KEYS_BACK", "true");
    SDL_SetHint("SDL_WEBOS_CURSOR_SLEEP_TIME", "5000");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }

    Uint32 flags = SDL_WINDOW_SHOWN;
    if (const char* size = std::getenv("WEBOS_HOME_WINDOW")) { // e.g. "1280x720" for desktop testing
        if (std::sscanf(size, "%dx%d", &width_, &height_) != 2)
            width_ = height_ = 0;
    }
    if (width_ <= 0 || height_ <= 0) {
#ifdef TARGET_WEBOS
        flags |= SDL_WINDOW_FULLSCREEN;
        SDL_DisplayMode mode;
        if (SDL_GetCurrentDisplayMode(0, &mode) == 0) {
            width_ = mode.w;
            height_ = mode.h;
        }
#endif
        if (width_ <= 0 || height_ <= 0) {
            width_ = 1920;
            height_ = 1080;
        }
    }

    window_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width_, height_, flags);
    if (!window_) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_)
        renderer_ = SDL_CreateRenderer(window_, -1, 0);
    if (!renderer_) {
        std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        return false;
    }
    // Scales the output (and mouse coordinates) if the real surface differs.
    SDL_RenderSetLogicalSize(renderer_, width_, height_);
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width_, height_);
    if (!texture_) {
        std::fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        return false;
    }

    // One full-screen buffer in DIRECT mode: LVGL only redraws dirty areas
    // in place, and we upload just the bounding box of those areas.
    framebuffer_.assign(static_cast<size_t>(width_) * height_ * 4, 0);
    display_ = lv_display_create(width_, height_);
    lv_display_set_color_format(display_, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display_, framebuffer_.data(), nullptr, static_cast<uint32_t>(framebuffer_.size()),
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display_, flushCb);
    lv_display_set_user_data(display_, this);

    pointer_ = lv_indev_create();
    lv_indev_set_type(pointer_, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer_, pointerReadCb);
    lv_indev_set_user_data(pointer_, this);
    lv_indev_set_display(pointer_, display_);
    return true;
}

void SdlBackend::handleEvent(const SDL_Event& e) {
    switch (e.type) {
    case SDL_MOUSEMOTION:
        mouseX_ = e.motion.x;
        mouseY_ = e.motion.y;
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        if (e.button.button == SDL_BUTTON_LEFT) {
            mouseX_ = e.button.x;
            mouseY_ = e.button.y;
            mouseDown_ = e.type == SDL_MOUSEBUTTONDOWN;
            // Read immediately so quick press/release pairs are not merged.
            lv_indev_read(pointer_);
        }
        break;
    case SDL_WINDOWEVENT:
        if (e.window.event == SDL_WINDOWEVENT_EXPOSED || e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
            lv_obj_invalidate(lv_display_get_screen_active(display_));
        break;
    default:
        break;
    }
}

void SdlBackend::flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t*) {
    auto* self = static_cast<SdlBackend*>(lv_display_get_user_data(disp));
    if (!self->hasDirty_) {
        self->dirty_ = *area;
        self->hasDirty_ = true;
    } else {
        self->dirty_.x1 = std::min(self->dirty_.x1, area->x1);
        self->dirty_.y1 = std::min(self->dirty_.y1, area->y1);
        self->dirty_.x2 = std::max(self->dirty_.x2, area->x2);
        self->dirty_.y2 = std::max(self->dirty_.y2, area->y2);
    }

    if (lv_display_flush_is_last(disp)) {
        const lv_area_t& d = self->dirty_;
        SDL_Rect rect{d.x1, d.y1, d.x2 - d.x1 + 1, d.y2 - d.y1 + 1};
        const int stride = self->width_ * 4;
        const uint8_t* src = self->framebuffer_.data() + static_cast<size_t>(d.y1) * stride + d.x1 * 4;
        SDL_UpdateTexture(self->texture_, &rect, src, stride);
        SDL_RenderClear(self->renderer_);
        SDL_RenderCopy(self->renderer_, self->texture_, nullptr, nullptr);
        SDL_RenderPresent(self->renderer_);
        self->hasDirty_ = false;
    }
    lv_display_flush_ready(disp);
}

void SdlBackend::pointerReadCb(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* self = static_cast<SdlBackend*>(lv_indev_get_user_data(indev));
    data->point.x = self->mouseX_;
    data->point.y = self->mouseY_;
    data->state = self->mouseDown_ ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
