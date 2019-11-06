// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_SCALE_FACTOR_H_
#define UI_BASE_RESOURCE_SCALE_FACTOR_H_

#include "ui/base/resource/data_pack_export.h"

namespace ui {

// Supported UI scale factors for the platform. This is used as an index
// into the array |kScaleFactorScales| which maps the enum value to a float.
// SCALE_FACTOR_NONE is used for density independent resources such as
// string, html/js files or an image that can be used for any scale factors
// (such as wallpapers).
enum ScaleFactor : int {
  SCALE_FACTOR_NONE = 0,
  SCALE_FACTOR_100P,
  SCALE_FACTOR_125P,
  SCALE_FACTOR_133P,
  SCALE_FACTOR_140P,
  SCALE_FACTOR_150P,
  SCALE_FACTOR_180P,
  SCALE_FACTOR_200P,
  SCALE_FACTOR_250P,
  SCALE_FACTOR_300P,

  NUM_SCALE_FACTORS  // This always appears last.
};

// Returns the image scale for the scale factor passed in.
UI_DATA_PACK_EXPORT float GetScaleForScaleFactor(ScaleFactor scale_factor);

}  // namespace ui

#endif  // UI_BASE_RESOURCE_SCALE_FACTOR_H_
