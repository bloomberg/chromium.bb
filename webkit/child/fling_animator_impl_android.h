// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_FLING_ANIMATOR_IMPL_ANDROID_H_
#define WEBKIT_CHILD_FLING_ANIMATOR_IMPL_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebGestureCurve.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "ui/gfx/point.h"
#include "ui/gfx/point_f.h"
#include "webkit/child/webkit_child_export.h"

namespace WebKit {
class WebGestureCurveTarget;
}

namespace webkit_glue {

class WEBKIT_CHILD_EXPORT FlingAnimatorImpl : public WebKit::WebGestureCurve {
 public:
  FlingAnimatorImpl();
  virtual ~FlingAnimatorImpl();

  static FlingAnimatorImpl* CreateAndroidGestureCurve(
      const WebKit::WebFloatPoint& velocity,
      const WebKit::WebSize&);

  virtual bool apply(double time,
                     WebKit::WebGestureCurveTarget* target);

  static bool RegisterJni(JNIEnv*);

 private:
  void StartFling(const gfx::PointF& velocity);
  // Returns true if the animation is not yet finished.
  bool UpdatePosition();
  gfx::Point GetCurrentPosition();
  float GetCurrentVelocity();
  virtual void CancelFling();

  bool is_active_;

  // Java OverScroller instance and methods.
  base::android::ScopedJavaGlobalRef<jobject> java_scroller_;

  gfx::Point last_position_;
  gfx::PointF last_velocity_;
  double last_time_;

  DISALLOW_COPY_AND_ASSIGN(FlingAnimatorImpl);
};

} // namespace webkit_glue

#endif // WEBKIT_CHILD_FLING_ANIMATOR_IMPL_ANDROID_H_
