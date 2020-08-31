// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_THROBBER_H_
#define CHROME_BROWSER_VR_ELEMENTS_THROBBER_H_

#include "cc/animation/transform_operation.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

// A throbber renders a fading and pulsing rect through animating element's
// scale and opacity.
class VR_UI_EXPORT Throbber : public Rect {
 public:
  Throbber();
  ~Throbber() override;

  void NotifyClientFloatAnimated(float value,
                                 int target_property_id,
                                 cc::KeyframeModel* keyframe_model) override;

  void SetCircleGrowAnimationEnabled(bool enabled);

 private:
  cc::TransformOperation scale_before_animation_;
  float opacity_before_animation_ = 0.f;

  DISALLOW_COPY_AND_ASSIGN(Throbber);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_THROBBER_H_
