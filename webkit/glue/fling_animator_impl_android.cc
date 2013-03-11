// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/fling_animator_impl_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "jni/OverScroller_jni.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGestureCurveTarget.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/vector2d.h"

namespace webkit_glue {

FlingAnimatorImpl::FlingAnimatorImpl()
    : is_active_(false) {
  // hold the global reference of the Java objects.
  JNIEnv* env = base::android::AttachCurrentThread();
  java_scroller_.Reset(JNI_OverScroller::Java_OverScroller_ConstructorAWOS_ACC(
      env,
      base::android::GetApplicationContext()));
}

FlingAnimatorImpl::~FlingAnimatorImpl()
{
}

//static
bool FlingAnimatorImpl::RegisterJni(JNIEnv* env) {
  return JNI_OverScroller::RegisterNativesImpl(env);
}

void FlingAnimatorImpl::StartFling(const gfx::PointF& velocity)
{
  // No bounds on the fling. See http://webkit.org/b/96403
  // Instead, use the largest possible bounds for minX/maxX/minY/maxY. The
  // compositor will ignore any attempt to scroll beyond the end of the page.

  DCHECK(velocity.x() || velocity.y());
  if (is_active_)
    CancelFling();

  is_active_ = true;

  JNIEnv* env = base::android::AttachCurrentThread();

  JNI_OverScroller::Java_OverScroller_flingV_I_I_I_I_I_I_I_I(
      env, java_scroller_.obj(), 0, 0,
      static_cast<int>(velocity.x()),
      static_cast<int>(velocity.y()),
      INT_MIN, INT_MAX, INT_MIN, INT_MAX);
}

void FlingAnimatorImpl::CancelFling()
{
  if (!is_active_)
    return;

  is_active_ = false;
  JNIEnv* env = base::android::AttachCurrentThread();
  JNI_OverScroller::Java_OverScroller_abortAnimation(env, java_scroller_.obj());
}

bool FlingAnimatorImpl::UpdatePosition()
{
  JNIEnv* env = base::android::AttachCurrentThread();
  bool result = JNI_OverScroller::Java_OverScroller_computeScrollOffset(
      env,
      java_scroller_.obj());
  return is_active_ = result;
}

gfx::Point FlingAnimatorImpl::GetCurrentPosition()
{
  JNIEnv* env = base::android::AttachCurrentThread();
  gfx::Point position(
      JNI_OverScroller::Java_OverScroller_getCurrX(env, java_scroller_.obj()),
      JNI_OverScroller::Java_OverScroller_getCurrY(env, java_scroller_.obj()));
  return position;
}

bool FlingAnimatorImpl::apply(double time,
                              WebKit::WebGestureCurveTarget* target) {
  if (!UpdatePosition())
    return false;

  gfx::Point current_position = GetCurrentPosition();
  gfx::Vector2d diff(current_position - last_position_);
  last_position_ = current_position;
  float dpi_scale = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay()
      .device_scale_factor();
  WebKit::WebFloatSize scroll_amount(diff.x() / dpi_scale,
                                     diff.y() / dpi_scale);
  // scrollBy() could delete this curve if the animation is over, so don't touch
  // any member variables after making that call.
  target->scrollBy(scroll_amount);
  return true;
}

FlingAnimatorImpl* FlingAnimatorImpl::CreateAndroidGestureCurve(
    const WebKit::WebFloatPoint& velocity,
    const WebKit::WebSize&) {
  FlingAnimatorImpl* gesture_curve = new FlingAnimatorImpl();
  gesture_curve->StartFling(velocity);
  return gesture_curve;
}

} // namespace webkit_glue
