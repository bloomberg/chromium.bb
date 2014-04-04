// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_UTIL_H_
#define UI_DISPLAY_DISPLAY_UTIL_H_

#include "ui/display/display_export.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

// Returns true if a given size is in the list of bogus sizes in mm that should
// be ignored.
DISPLAY_EXPORT bool IsDisplaySizeBlackListed(const gfx::Size& physical_size);

}  // namespace ui

#endif  // UI_DISPLAY_DISPLAY_UTIL_H_
