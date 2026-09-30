#pragma once

#include <cstdint>
#include <vector>

#include <SDL.h>
#include <lvgl.h>

// Fullscreen SDL2 window acting as LVGL display, plus the Magic Remote /
// mouse pointer as LVGL pointer input. Key events are left to the caller.
class SdlBackend {
public:
    SdlBackend() = default;
    ~SdlBackend();
    SdlBackend(const SdlBackend&) = delete;
    SdlBackend& operator=(const SdlBackend&) = delete;

    // Must be called after lv_init().
    bool init(const char* title);

    // Feeds mouse events into the LVGL pointer device.
    void handleEvent(const SDL_Event& e);

    lv_display_t* display() const { return display_; }

private:
    static void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px);
    static void pointerReadCb(lv_indev_t* indev, lv_indev_data_t* data);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> framebuffer_;
    lv_display_t* display_ = nullptr;
    lv_indev_t* pointer_ = nullptr;

    lv_area_t dirty_{};
    bool hasDirty_ = false;

    int mouseX_ = 0;
    int mouseY_ = 0;
    bool mouseDown_ = false;
};
