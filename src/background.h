#pragma once

#include <cstdint>
#include <string>

// Decodes a JPEG (baseline or progressive) and scales it to "cover" a
// width x height XRGB8888 buffer: fills it completely, cropping the overflow.
// Returns false and sets `error` if the file can't be decoded.
bool renderBackground(const std::string& path, uint8_t* dst, int width, int height, int stride,
                      std::string& error);
