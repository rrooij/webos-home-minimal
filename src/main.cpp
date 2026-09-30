#include <algorithm>
#include <cstdio>

#include <SDL.h>
#include <lvgl.h>

#include "apps.h"
#include "config.h"
#include "launcher_ui.h"
#include "luna.h"
#include "sdl_backend.h"

namespace {

// LG's SDL reports the remote's Back button with this webOS-specific scancode.
constexpr int kWebosBackScancode = 482;
// Don't re-query the app list more often than this when regaining focus.
constexpr Uint32 kRefreshCooldownMs = 2000;

uint32_t tickCb() { return SDL_GetTicks(); }

} // namespace

int main(int argc, char** argv) {
    Config cfg = loadConfig(argc, argv);
    LunaClient luna(cfg.lunaSend, cfg.useHbchannel);
    AppSource source(cfg, luna);

    lv_init();
    lv_tick_set_cb(tickCb);

    SdlBackend backend;
    if (!backend.init("Home"))
        return 1;

    LauncherUi ui(backend.display(), [&](const AppEntry& app) {
        if (!source.launch(app))
            std::fprintf(stderr, "failed to launch %s\n", app.id.c_str());
    }, cfg.background, cfg.iconPosition);
    auto refresh = [&] {
        auto apps = source.list();
        ui.setApps(std::move(apps), source.lastError());
    };
    refresh();
    Uint32 lastRefresh = SDL_GetTicks();

    bool running = true;
    while (running) {
        uint32_t wait = lv_timer_handler();
        SDL_Event e;
        // Sleep until the next LVGL timer is due or an event arrives.
        if (!SDL_WaitEventTimeout(&e, static_cast<int>(std::clamp<uint32_t>(wait, 1, 500))))
            continue;
        do {
            backend.handleEvent(e);
            switch (e.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                // Back is swallowed on purpose: a home screen has nowhere to go back to.
                if (e.key.keysym.scancode == kWebosBackScancode)
                    break;
                switch (e.key.keysym.sym) {
                case SDLK_LEFT:
                    ui.move(-1);
                    break;
                case SDLK_RIGHT:
                    ui.move(1);
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    if (!e.key.repeat)
                        ui.activate();
                    break;
#ifndef TARGET_WEBOS
                case SDLK_ESCAPE:
                    running = false;
                    break;
#endif
                default:
                    break;
                }
                break;
            case SDL_MOUSEWHEEL:
                ui.move(e.wheel.y > 0 ? -1 : 1);
                break;
            case SDL_WINDOWEVENT:
                // Back from another app: pick up installs/removals.
                if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED &&
                    SDL_GetTicks() - lastRefresh > kRefreshCooldownMs) {
                    refresh();
                    lastRefresh = SDL_GetTicks();
                }
                break;
            default:
                break;
            }
        } while (SDL_PollEvent(&e));
    }
    return 0;
}
