#include "background.h"

#include <algorithm>
#include <cmath>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#include <stb_image.h>

namespace {

struct Rgb {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> px; // tightly packed RGB
};

// 2x2 box downscale; repeated until the image is less than 2x the target, so
// the final bilinear pass doesn't alias on big photos.
Rgb halve(const Rgb& in) {
    Rgb out;
    out.w = in.w / 2;
    out.h = in.h / 2;
    out.px.resize(static_cast<size_t>(out.w) * out.h * 3);
    for (int y = 0; y < out.h; ++y) {
        const uint8_t* r0 = &in.px[static_cast<size_t>(2 * y) * in.w * 3];
        const uint8_t* r1 = r0 + static_cast<size_t>(in.w) * 3;
        uint8_t* o = &out.px[static_cast<size_t>(y) * out.w * 3];
        for (int x = 0; x < out.w; ++x)
            for (int c = 0; c < 3; ++c)
                o[x * 3 + c] = static_cast<uint8_t>(
                    (r0[6 * x + c] + r0[6 * x + 3 + c] + r1[6 * x + c] + r1[6 * x + 3 + c] + 2) / 4);
    }
    return out;
}

} // namespace

bool renderBackground(const std::string& path, uint8_t* dst, int width, int height, int stride,
                      std::string& error) {
    int w = 0, h = 0, channels = 0;
    uint8_t* data = stbi_load(path.c_str(), &w, &h, &channels, 3);
    if (!data) {
        error = stbi_failure_reason() ? stbi_failure_reason() : "decode failed";
        return false;
    }
    Rgb img;
    img.w = w;
    img.h = h;
    img.px.assign(data, data + static_cast<size_t>(w) * h * 3);
    stbi_image_free(data);

    auto coverScale = [&] { return std::max(float(width) / img.w, float(height) / img.h); };
    while (coverScale() <= 0.5f && img.w >= 2 && img.h >= 2)
        img = halve(img);

    // Bilinear sample of the centered, cropped region.
    const float s = coverScale();
    const float x0 = (img.w - width / s) / 2;
    const float y0 = (img.h - height / s) / 2;

    std::vector<int> xi(width);
    std::vector<float> xf(width);
    for (int x = 0; x < width; ++x) {
        float fx = std::clamp(x0 + (x + 0.5f) / s - 0.5f, 0.0f, float(img.w - 1));
        xi[x] = std::min(int(fx), std::max(img.w - 2, 0));
        xf[x] = img.w > 1 ? fx - xi[x] : 0.0f;
    }

    for (int y = 0; y < height; ++y) {
        float fy = std::clamp(y0 + (y + 0.5f) / s - 0.5f, 0.0f, float(img.h - 1));
        int yi = std::min(int(fy), std::max(img.h - 2, 0));
        float wy = img.h > 1 ? fy - yi : 0.0f;
        const uint8_t* r0 = &img.px[static_cast<size_t>(yi) * img.w * 3];
        const uint8_t* r1 = img.h > 1 ? r0 + static_cast<size_t>(img.w) * 3 : r0;
        uint8_t* out = dst + static_cast<size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            const int i = xi[x] * 3;
            const int j = img.w > 1 ? i + 3 : i;
            const float wx = xf[x];
            uint8_t rgb[3];
            for (int c = 0; c < 3; ++c) {
                float top = r0[i + c] + (r0[j + c] - r0[i + c]) * wx;
                float bottom = r1[i + c] + (r1[j + c] - r1[i + c]) * wx;
                rgb[c] = static_cast<uint8_t>(std::lround(top + (bottom - top) * wy));
            }
            // XRGB8888 in memory: B, G, R, X
            out[x * 4 + 0] = rgb[2];
            out[x * 4 + 1] = rgb[1];
            out[x * 4 + 2] = rgb[0];
            out[x * 4 + 3] = 0xff;
        }
    }
    return true;
}
