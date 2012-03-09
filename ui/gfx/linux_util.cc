// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux_util.h"

#include "base/string_util.h"

namespace {

// Common implementation of ConvertAcceleratorsFromWindowsStyle() and
// RemoveWindowsStyleAccelerators().
// Replaces all ampersands (as used in our grd files to indicate mnemonics)
// to |target|, except ampersands appearing in pairs which are replaced by
// a single ampersand. Any underscores get replaced with two underscores as
// is needed by GTK.
std::string ConvertAmpersandsTo(const std::string& label,
                                const std::string& target) {
  std::string ret;
  ret.reserve(label.length() * 2);
  for (size_t i = 0; i < label.length(); ++i) {
    if ('_' == label[i]) {
      ret.push_back('_');
      ret.push_back('_');
    } else if ('&' == label[i]) {
      if (i + 1 < label.length() && '&' == label[i + 1]) {
        ret.push_back('&');
        ++i;
      } else {
        ret.append(target);
      }
    } else {
      ret.push_back(label[i]);
    }
  }

  return ret;
}

}  // namespace

namespace gfx {

std::string ConvertAcceleratorsFromWindowsStyle(const std::string& label) {
  return ConvertAmpersandsTo(label, "_");
}

std::string RemoveWindowsStyleAccelerators(const std::string& label) {
  return ConvertAmpersandsTo(label, "");
}

// Replaces all ampersands in |label| with two ampersands. This effectively
// escapes strings for later processing by ConvertAmpersandsTo(), so that
// ConvertAmpersandsTo(EscapeWindowsStyleAccelerators(x), *) is |x| with
// underscores doubled, making the string that appears to the user just |x|.
std::string EscapeWindowsStyleAccelerators(const std::string& label) {
  std::string ret;
  ReplaceChars(label, "&", "&&", &ret);
  return ret;
}

uint8_t* BGRAToRGBA(const uint8_t* pixels, int width, int height, int stride) {
  if (stride == 0)
    stride = width * 4;

  uint8_t* new_pixels = static_cast<uint8_t*>(malloc(height * stride));

  // We have to copy the pixels and swap from BGRA to RGBA.
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      int idx = i * stride + j * 4;
      new_pixels[idx] = pixels[idx + 2];
      new_pixels[idx + 1] = pixels[idx + 1];
      new_pixels[idx + 2] = pixels[idx];
      new_pixels[idx + 3] = pixels[idx + 3];
    }
  }

  return new_pixels;
}

}  // namespace gfx
