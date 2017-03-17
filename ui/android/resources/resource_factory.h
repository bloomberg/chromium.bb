// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_RESOURCE_FACTORY_H_
#define UI_ANDROID_RESOURCES_RESOURCE_FACTORY_H_

#include "base/android/jni_android.h"

namespace ui {

bool RegisterResourceFactory(JNIEnv* env);

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_RESOURCE_FACTORY_H_
