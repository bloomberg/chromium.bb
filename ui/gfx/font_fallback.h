// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_FALLBACK_H_
#define UI_GFX_FONT_FALLBACK_H_

#include <string>
#include <vector>

#include "ui/gfx/gfx_export.h"

namespace gfx {

// Given a font family name, returns the names of font families that are
// suitable for fallback.
GFX_EXPORT std::vector<std::string> GetFallbackFontFamilies(
    const std::string& font_family);

}  // namespace gfx

#endif  // UI_GFX_FONT_FALLBACK_H_
