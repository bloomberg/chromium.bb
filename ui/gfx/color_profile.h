// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_PROFILE_H_
#define UI_GFX_COLOR_PROFILE_H_

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

class GFX_EXPORT ColorProfile {
 public:
  ColorProfile();
  ColorProfile(ColorProfile&& other);
  ColorProfile(const ColorProfile& other);
  ColorProfile& operator=(const ColorProfile& other);
  ~ColorProfile();

  const std::vector<char>& profile() const { return profile_; }

#if defined(OS_WIN)
  // This will read color profile information from disk and cache the results
  // for the other functions to read. This should not be called on the UI or IO
  // thread.
  static void UpdateCachedProfilesOnBackgroundThread();
  static bool CachedProfilesNeedUpdate();
#endif

  // Returns the color profile of the monitor that can best represent color.
  // This profile should be used for creating content that does not know on
  // which monitor it will be displayed.
  static ColorProfile GetFromBestMonitor();

  static bool IsValidProfileLength(size_t length);

 private:
  std::vector<char> profile_;
};

}  // namespace gfx

#endif  // UI_GFX_COLOR_PROFILE_H_
