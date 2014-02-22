// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_FLING_ANIMATOR_IMPL_ANDROID_H_
#define WEBKIT_CHILD_FLING_ANIMATOR_IMPL_ANDROID_H_


#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebGestureCurve.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "ui/gfx/android/scroller.h"
#include "ui/gfx/point_f.h"
#include "webkit/child/webkit_child_export.h"

namespace blink {
class WebGestureCurveTarget;
}

namespace webkit_glue {

class WEBKIT_CHILD_EXPORT FlingAnimatorImpl : public blink::WebGestureCurve {
 public:
  FlingAnimatorImpl();
  virtual ~FlingAnimatorImpl();

  static FlingAnimatorImpl* CreateAndroidGestureCurve(
      const blink::WebFloatPoint& velocity,
      const blink::WebSize&);

  virtual bool apply(double time, blink::WebGestureCurveTarget* target);

 private:
  void StartFling(const gfx::PointF& velocity);
  void CancelFling();

  bool is_active_;

  gfx::Scroller scroller_;

  gfx::PointF last_position_;

  DISALLOW_COPY_AND_ASSIGN(FlingAnimatorImpl);
};

}  // namespace webkit_glue

#endif  // WEBKIT_CHILD_FLING_ANIMATOR_IMPL_ANDROID_H_
