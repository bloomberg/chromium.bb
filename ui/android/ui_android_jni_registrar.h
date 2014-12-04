// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_UI_ANDROID_JNI_REGISTRAR_H_
#define UI_ANDROID_UI_ANDROID_JNI_REGISTRAR_H_

#include <jni.h>

#include "ui/android/ui_android_export.h"

namespace ui {

// Register all JNI bindings necessary for chrome.
UI_ANDROID_EXPORT bool RegisterUIAndroidJni(JNIEnv* env);

}  // namespace ui

#endif  // UI_ANDROID_UI_ANDROID_JNI_REGISTRAR_H_
