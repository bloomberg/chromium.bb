// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_util.h"

#include "base/logging.h"

namespace ui {

namespace {

// A list of bogus sizes in mm that should be ignored.
// See crbug.com/136533. The first element maintains the minimum
// size required to be valid size.
const int kInvalidDisplaySizeList[][2] = {
  {40, 30},
  {50, 40},
  {160, 90},
  {160, 100},
};

}  // namespace

bool IsDisplaySizeBlackListed(const gfx::Size& physical_size) {
  // Ignore if the reported display is smaller than minimum size.
  if (physical_size.width() <= kInvalidDisplaySizeList[0][0] ||
      physical_size.height() <= kInvalidDisplaySizeList[0][1]) {
    LOG(WARNING) << "Smaller than minimum display size";
    return true;
  }
  for (size_t i = 1; i < arraysize(kInvalidDisplaySizeList); ++i) {
    const gfx::Size size(kInvalidDisplaySizeList[i][0],
                         kInvalidDisplaySizeList[i][1]);
    if (physical_size == size) {
      LOG(WARNING) << "Black listed display size detected:" << size.ToString();
      return true;
    }
  }
  return false;
}

}  // namespace ui
