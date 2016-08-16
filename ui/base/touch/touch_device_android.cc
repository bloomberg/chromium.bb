// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_device.h"

#include "base/android/context_utils.h"
#include "base/logging.h"
#include "jni/TouchDevice_jni.h"

using base::android::AttachCurrentThread;
using base::android::GetApplicationContext;

namespace ui {

TouchScreensAvailability GetTouchScreensAvailability() {
  return TouchScreensAvailability::ENABLED;
}

int MaxTouchPoints() {
  return Java_TouchDevice_maxTouchPoints(AttachCurrentThread(),
                                         GetApplicationContext());
}

int GetAvailablePointerTypes() {
  return Java_TouchDevice_availablePointerTypes(AttachCurrentThread(),
                                                GetApplicationContext());
}

PointerType GetPrimaryPointerType() {
  int available_pointer_types = GetAvailablePointerTypes();
  if (available_pointer_types & POINTER_TYPE_COARSE)
    return POINTER_TYPE_COARSE;
  if (available_pointer_types & POINTER_TYPE_FINE)
    return POINTER_TYPE_FINE;
  DCHECK_EQ(available_pointer_types, POINTER_TYPE_NONE);
  return POINTER_TYPE_NONE;
}

int GetAvailableHoverTypes() {
  return Java_TouchDevice_availableHoverTypes(AttachCurrentThread(),
                                              GetApplicationContext());
}

HoverType GetPrimaryHoverType() {
  int available_hover_types = GetAvailableHoverTypes();
  if (available_hover_types & HOVER_TYPE_ON_DEMAND)
    return HOVER_TYPE_ON_DEMAND;
  if (available_hover_types & HOVER_TYPE_HOVER)
    return HOVER_TYPE_HOVER;
  DCHECK_EQ(available_hover_types, HOVER_TYPE_NONE);
  return HOVER_TYPE_NONE;
}

}  // namespace ui
