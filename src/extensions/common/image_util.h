// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_IMAGE_UTIL_H_
#define EXTENSIONS_COMMON_IMAGE_UTIL_H_

#include <string>

class SkBitmap;

typedef unsigned int SkColor;

namespace base {
class FilePath;
}

// This file contains various utility functions for extension images and colors.
namespace extensions {
namespace image_util {

// Parses a CSS-style color string from hex (3- or 6-digit) or HSL(A) format.
// Returns true on success.
bool ParseCssColorString(const std::string& color_string, SkColor* result);

// Parses a RGB or RGBA string like #FF9982CC, #FF9982, #EEEE, or #EEE to a
// color. Returns true for success.
bool ParseHexColorString(const std::string& color_string, SkColor* result);

// Creates a string like #FF9982 from a color.
std::string GenerateHexColorString(SkColor color);

// Parses rgb() or rgba() string to a color. Returns true for success.
bool ParseRgbColorString(const std::string& color_string, SkColor* result);

// Parses hsl() or hsla() string to a SkColor. Returns true for success.
bool ParseHslColorString(const std::string& color_string, SkColor* result);

// Returns whether an icon image is considered to be visible in its display
// context.
bool IsIconSufficientlyVisible(const SkBitmap& bitmap);

// Returns whether an icon image is considered to be visible in its display
// context.
bool IsIconAtPathSufficientlyVisible(const base::FilePath& path);

// Load a PNG image from a file into the destination bitmap.
bool LoadPngFromFile(const base::FilePath& path, SkBitmap* dst);

}  // namespace image_util
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_IMAGE_UTIL_H_
