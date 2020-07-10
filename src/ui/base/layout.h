// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_LAYOUT_H_
#define UI_BASE_LAYOUT_H_

#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/base/resource/scale_factor.h"
#include "ui/base/ui_base_export.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

// Changes the value of GetSupportedScaleFactors() to |scale_factors|.
// Use ScopedSetSupportedScaleFactors for unit tests as not to affect the
// state of other tests.
UI_BASE_EXPORT void SetSupportedScaleFactors(
    const std::vector<ScaleFactor>& scale_factors);

// Returns a vector with the scale factors which are supported by this
// platform, in ascending order.
UI_BASE_EXPORT const std::vector<ScaleFactor>& GetSupportedScaleFactors();

// Returns the supported ScaleFactor which most closely matches |scale|.
// Converting from float to ScaleFactor is inefficient and should be done as
// little as possible.
UI_BASE_EXPORT ScaleFactor GetSupportedScaleFactor(float image_scale);

// Returns the ScaleFactor used by |view|.
UI_BASE_EXPORT float GetScaleFactorForNativeView(gfx::NativeView view);

// Returns true if the scale passed in is the list of supported scales for
// the platform.
UI_BASE_EXPORT bool IsSupportedScale(float scale);

namespace test {
// Class which changes the value of GetSupportedScaleFactors() to
// |new_scale_factors| for the duration of its lifetime.
class UI_BASE_EXPORT ScopedSetSupportedScaleFactors {
 public:
  explicit ScopedSetSupportedScaleFactors(
      const std::vector<ui::ScaleFactor>& new_scale_factors);
  ~ScopedSetSupportedScaleFactors();

 private:
  std::vector<ui::ScaleFactor>* original_scale_factors_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSetSupportedScaleFactors);
};

}  // namespace test

}  // namespace ui

#endif  // UI_BASE_LAYOUT_H_
