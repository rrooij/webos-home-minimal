#pragma once

#include <functional>
#include <vector>

#include <lvgl.h>

#include "apps.h"

// A single horizontal row of app tiles. The selected tile is kept centered;
// the remote's left/right keys move the selection, OK or a pointer click
// launches.
class LauncherUi {
public:
    using LaunchFn = std::function<void(const AppEntry&)>;

    // backgroundImage: optional JPEG path, scaled to cover the screen.
    // position: vertical placement of the icons (Center, Top, Bottom).
    LauncherUi(lv_display_t* disp, LaunchFn onLaunch, const std::string& backgroundImage = {},
               IconPosition position = IconPosition::Center);

    // Rebuilds the tiles, keeping the current selection if it still exists.
    // `emptyReason` is shown when there are no apps.
    void setApps(std::vector<AppEntry> apps, const std::string& emptyReason = {});

    void move(int delta);
    void activate();

private:
    static void tileClickedCb(lv_event_t* e);

    void select(int index, bool animate);
    void createBackground(lv_obj_t* screen, const std::string& path);
    int px(int v) const { return v * height_ / 1080; }

    LaunchFn onLaunch_;
    int width_;
    int height_;
    int tileSize_;
    lv_align_t titleAlign_ = LV_ALIGN_OUT_BOTTOM_MID;
    int titleYOfs_ = 0;

    lv_draw_buf_t* backgroundBuf_ = nullptr;
    lv_obj_t* row_;
    lv_obj_t* title_;
    lv_obj_t* empty_;
    lv_obj_t* emptyReason_;

    lv_style_t tileStyle_;
    lv_style_t selectedStyle_;
    lv_style_transition_dsc_t transition_;

    std::vector<AppEntry> apps_;
    std::vector<lv_obj_t*> tiles_;
    int selected_ = -1;
};
