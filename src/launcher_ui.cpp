#include "launcher_ui.h"

#include "background.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>

namespace {

constexpr uint32_t kBackground = 0x000000;
constexpr uint32_t kTileDefault = 0x2a2f38;

const lv_style_prop_t kTransitionProps[] = {
    LV_STYLE_TRANSFORM_SCALE_X, LV_STYLE_TRANSFORM_SCALE_Y, LV_STYLE_OUTLINE_WIDTH, LV_STYLE_PROP_INV};

} // namespace

LauncherUi::LauncherUi(lv_display_t* disp, LaunchFn onLaunch, const std::string& backgroundImage,
                       IconPosition position)
    : onLaunch_(std::move(onLaunch)),
      width_(lv_display_get_horizontal_resolution(disp)),
      height_(lv_display_get_vertical_resolution(disp)),
      tileSize_(px(220)) {
    lv_obj_t* screen = lv_display_get_screen_active(disp);
    lv_obj_set_style_bg_color(screen, lv_color_hex(kBackground), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    if (!backgroundImage.empty())
        createBackground(screen, backgroundImage);

    lv_style_transition_dsc_init(&transition_, kTransitionProps, lv_anim_path_ease_out, 150, 0, nullptr);

    lv_style_init(&tileStyle_);
    lv_style_set_radius(&tileStyle_, px(28));
    lv_style_set_border_width(&tileStyle_, 0);
    lv_style_set_pad_all(&tileStyle_, px(28));
    lv_style_set_bg_opa(&tileStyle_, LV_OPA_COVER);
    lv_style_set_outline_color(&tileStyle_, lv_color_white());
    lv_style_set_outline_pad(&tileStyle_, px(6));
    lv_style_set_outline_width(&tileStyle_, 0);
    lv_style_set_transform_pivot_x(&tileStyle_, LV_PCT(50));
    lv_style_set_transform_pivot_y(&tileStyle_, LV_PCT(50));
    lv_style_set_transition(&tileStyle_, &transition_);

    lv_style_init(&selectedStyle_);
    lv_style_set_outline_width(&selectedStyle_, px(6));
    lv_style_set_transform_scale(&selectedStyle_, 300); // 256 = 100%

    // Horizontal row; side padding lets the first and last tile be centered.
    row_ = lv_obj_create(screen);
    lv_obj_remove_style_all(row_);
    lv_obj_set_size(row_, width_, tileSize_ * 3 / 2);
    switch (position) {
    case IconPosition::Top:
        lv_obj_align(row_, LV_ALIGN_TOP_MID, 0, px(40));
        titleAlign_ = LV_ALIGN_OUT_BOTTOM_MID;
        titleYOfs_ = px(24);
        break;
    case IconPosition::Bottom:
        lv_obj_align(row_, LV_ALIGN_BOTTOM_MID, 0, -px(40));
        titleAlign_ = LV_ALIGN_OUT_TOP_MID;
        titleYOfs_ = -px(24);
        break;
    case IconPosition::Center:
    default:
        lv_obj_align(row_, LV_ALIGN_CENTER, 0, -px(40));
        titleAlign_ = LV_ALIGN_OUT_BOTTOM_MID;
        titleYOfs_ = px(24);
        break;
    }
    lv_obj_set_flex_flow(row_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_, px(56), 0);
    lv_obj_set_style_pad_hor(row_, (width_ - tileSize_) / 2, 0);
    lv_obj_set_scroll_dir(row_, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(row_, LV_SCROLLBAR_MODE_OFF);

    title_ = lv_label_create(screen);
    lv_obj_set_style_text_font(title_, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(title_, lv_color_white(), 0);
    lv_obj_align_to(title_, row_, titleAlign_, 0, titleYOfs_);
    lv_label_set_text(title_, "");

    empty_ = lv_label_create(screen);
    lv_obj_set_style_text_color(empty_, lv_color_hex(0x8a93a0), 0);
    lv_label_set_text(empty_, "No applications found");
    lv_obj_align(empty_, LV_ALIGN_CENTER, 0, -px(60));
    lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);

    emptyReason_ = lv_label_create(screen);
    lv_obj_set_width(emptyReason_, width_ - px(160));
    lv_label_set_long_mode(emptyReason_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(emptyReason_, lv_color_hex(0x5d6570), 0);
    lv_obj_set_style_text_align(emptyReason_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(emptyReason_, empty_, LV_ALIGN_OUT_BOTTOM_MID, 0, px(24));
    lv_label_set_text(emptyReason_, "");
    lv_obj_add_flag(emptyReason_, LV_OBJ_FLAG_HIDDEN);
}

// Decodes and scales the image once into a screen-sized canvas, so redraws
// are a plain copy.
void LauncherUi::createBackground(lv_obj_t* screen, const std::string& path) {
    backgroundBuf_ = lv_draw_buf_create(width_, height_, LV_COLOR_FORMAT_XRGB8888, LV_STRIDE_AUTO);
    if (!backgroundBuf_)
        return;
    std::string error;
    if (!renderBackground(path, backgroundBuf_->data, width_, height_,
                          static_cast<int>(backgroundBuf_->header.stride), error)) {
        std::fprintf(stderr, "background: %s: %s\n", path.c_str(), error.c_str());
        lv_draw_buf_destroy(backgroundBuf_);
        backgroundBuf_ = nullptr;
        // Say so on screen; stderr is hard to see on the TV.
        lv_obj_t* label = lv_label_create(screen);
        lv_label_set_text_fmt(label, "%s: %s", path.c_str(), error.c_str());
        lv_obj_set_style_text_color(label, lv_color_hex(0x5d6570), 0);
        lv_obj_align(label, LV_ALIGN_BOTTOM_LEFT, px(40), -px(40));
        return;
    }
    lv_obj_t* canvas = lv_canvas_create(screen);
    lv_canvas_set_draw_buf(canvas, backgroundBuf_);
    lv_obj_center(canvas);
}

void LauncherUi::setApps(std::vector<AppEntry> apps, const std::string& emptyReason) {
    std::string selectedId = selected_ >= 0 ? apps_[selected_].id : std::string();

    lv_obj_clean(row_);
    tiles_.clear();
    apps_ = std::move(apps);
    selected_ = -1;

    int newSelection = 0;
    for (size_t i = 0; i < apps_.size(); ++i) {
        const AppEntry& app = apps_[i];
        if (app.id == selectedId)
            newSelection = static_cast<int>(i);

        lv_obj_t* tile = lv_obj_create(row_);
        lv_obj_remove_style_all(tile);
        lv_obj_add_style(tile, &tileStyle_, 0);
        lv_obj_add_style(tile, &selectedStyle_, LV_STATE_CHECKED);
        lv_obj_set_size(tile, tileSize_, tileSize_);
        lv_obj_set_style_bg_color(tile, lv_color_hex(app.color.value_or(kTileDefault)), 0);
        lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(tile, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        lv_obj_add_event_cb(tile, tileClickedCb, LV_EVENT_CLICKED, this);

        if (!app.icon.empty()) {
            lv_obj_t* img = lv_image_create(tile);
            std::string src = "A:" + app.icon;
            lv_image_set_src(img, src.c_str());
            lv_obj_set_size(img, LV_PCT(100), LV_PCT(100));
            lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CENTER);
            // Fit the icon into the tile, whatever size the app ships.
            lv_image_header_t header;
            if (lv_image_decoder_get_info(src.c_str(), &header) == LV_RESULT_OK && header.w > 0 && header.h > 0) {
                int32_t box = tileSize_ - 2 * px(28);
                int32_t w = header.w, h = header.h;
                lv_image_set_scale(img, static_cast<uint32_t>(std::min(box * 256 / w, box * 256 / h)));
            }
            lv_obj_center(img);
        } else {
            // No icon: show the first letter of the title.
            lv_obj_t* letter = lv_label_create(tile);
            char text[2] = {static_cast<char>(std::toupper(static_cast<unsigned char>(app.title[0]))), '\0'};
            lv_label_set_text(letter, text);
            lv_obj_set_style_text_font(letter, &lv_font_montserrat_48, 0);
            uint32_t bg = app.color.value_or(kTileDefault);
            int luma = (299 * ((bg >> 16) & 0xff) + 587 * ((bg >> 8) & 0xff) + 114 * (bg & 0xff)) / 1000;
            lv_obj_set_style_text_color(letter, luma > 160 ? lv_color_hex(kBackground) : lv_color_white(), 0);
            lv_obj_center(letter);
        }
        tiles_.push_back(tile);
    }

    if (apps_.empty()) {
        lv_obj_remove_flag(empty_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(emptyReason_, emptyReason.c_str());
        lv_obj_align_to(emptyReason_, empty_, LV_ALIGN_OUT_BOTTOM_MID, 0, px(24));
        lv_obj_remove_flag(emptyReason_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(title_, "");
        return;
    }
    lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(emptyReason_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(row_);
    select(newSelection, false);
}

void LauncherUi::move(int delta) {
    if (!tiles_.empty())
        select(std::clamp(selected_ + delta, 0, static_cast<int>(tiles_.size()) - 1), true);
}

void LauncherUi::activate() {
    if (selected_ >= 0 && onLaunch_)
        onLaunch_(apps_[selected_]);
}

void LauncherUi::select(int index, bool animate) {
    if (selected_ >= 0 && selected_ != index)
        lv_obj_remove_state(tiles_[selected_], LV_STATE_CHECKED);
    selected_ = index;

    lv_obj_t* tile = tiles_[index];
    lv_obj_add_state(tile, LV_STATE_CHECKED);
    lv_label_set_text(title_, apps_[index].title.c_str());
    lv_obj_align_to(title_, row_, titleAlign_, 0, titleYOfs_);

    // lv_obj_get_x() is measured from the row's unscrolled content area (after
    // the left padding). That padding is (width - tile) / 2, so scrolling by
    // exactly x puts the tile in the middle of the screen.
    lv_obj_scroll_to_x(row_, lv_obj_get_x(tile), animate ? LV_ANIM_ON : LV_ANIM_OFF);
}

void LauncherUi::tileClickedCb(lv_event_t* e) {
    auto* self = static_cast<LauncherUi*>(lv_event_get_user_data(e));
    auto* tile = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(tile)));
    self->select(index, true);
    self->activate();
}
