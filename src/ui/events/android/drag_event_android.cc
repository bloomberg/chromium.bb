// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/drag_event_android.h"
#include "base/android/jni_android.h"

using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;

namespace ui {

DragEventAndroid::DragEventAndroid(
    JNIEnv* env,
    int action,
    const gfx::PointF& location,
    const gfx::PointF& screen_location,
    const std::vector<base::string16>& mime_types,
    jstring content)
    : action_(action),
      location_(location),
      screen_location_(screen_location),
      mime_types_(mime_types) {
  content_.Reset(env, content);
}

DragEventAndroid::~DragEventAndroid() {}

const gfx::Point DragEventAndroid::GetLocation() const {
  return gfx::ToFlooredPoint(location_);
}

const gfx::Point DragEventAndroid::GetScreenLocation() const {
  return gfx::ToFlooredPoint(screen_location_);
}

ScopedJavaLocalRef<jstring> DragEventAndroid::GetJavaContent() const {
  return ScopedJavaLocalRef<jstring>(content_);
}

std::unique_ptr<DragEventAndroid> DragEventAndroid::CreateFor(
    const gfx::PointF& new_location) const {
  gfx::PointF new_screen_location =
      new_location + (screen_location_f() - location_f());
  JNIEnv* env = AttachCurrentThread();
  return std::unique_ptr<DragEventAndroid>(
      new DragEventAndroid(env, action_, new_location, new_screen_location,
                           mime_types_, content_.obj()));
}

}  // namespace ui
