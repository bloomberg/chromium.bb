// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_METRO_VIEWER_IME_TYPES_H_
#define UI_METRO_VIEWER_IME_TYPES_H_

#include <stdint.h>

#include <vector>

#include "base/strings/string16.h"

namespace metro_viewer {

// An equivalent to ui::CompositionUnderline defined in
// "ui/base/ime/composition_underline.h". Redefined here to avoid dependency
// on ui.gyp from metro_driver.gyp.
struct UnderlineInfo {
  UnderlineInfo();
  int32_t start_offset;
  int32_t end_offset;
  bool thick;
};

// An equivalent to ui::CompositionText defined in
// "ui/base/ime/composition_text.h". Redefined here to avoid dependency
// on ui.gyp from metro_driver.gyp.
struct Composition {
  Composition();
  ~Composition();
  base::string16 text;
  int32_t selection_start;
  int32_t selection_end;
  std::vector<UnderlineInfo> underlines;
};

// An equivalent to Win32 RECT structure. This can be gfx::Rect but redefined
// here to avoid dependency on gfx.gyp from metro_driver.gyp.
struct CharacterBounds {
  CharacterBounds();
  int32_t left;
  int32_t top;
  int32_t right;
  int32_t bottom;
};

}  // namespace metro_viewer

#endif  // UI_METRO_VIEWER_IME_TYPES_H_
