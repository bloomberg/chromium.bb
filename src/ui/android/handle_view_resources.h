// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_HANDLE_VIEW_RESOURCES_H_
#define UI_ANDROID_HANDLE_VIEW_RESOURCES_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/touch_selection/touch_handle.h"

using base::android::JavaRef;

namespace ui {

// Bridge to Java bitmap resources for selection handle.
class UI_ANDROID_EXPORT HandleViewResources {
 public:
  HandleViewResources();
  void LoadIfNecessary(const JavaRef<jobject>& context);
  const SkBitmap& GetBitmap(ui::TouchHandleOrientation orientation);
  float GetDrawableHorizontalPaddingRatio() const;

 private:
  SkBitmap left_bitmap_;
  SkBitmap right_bitmap_;
  SkBitmap center_bitmap_;
  float drawable_horizontal_padding_ratio_;
  bool loaded_ = false;

  DISALLOW_COPY_AND_ASSIGN(HandleViewResources);
};

}  // namespace ui

#endif  // UI_ANDROID_HANDLE_VIEW_RESOURCES_H_
