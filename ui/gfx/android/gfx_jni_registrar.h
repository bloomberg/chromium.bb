// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANDROID_GFX_JNI_REGISTRAR_H_
#define UI_GFX_ANDROID_GFX_JNI_REGISTRAR_H_

#include <jni.h>

#include "ui/base/ui_export.h"

namespace gfx {

// Register all JNI bindings necessary for gfx.
UI_EXPORT bool RegisterJni(JNIEnv* env);

}  // namespace gfx

#endif  // UI_GFX_ANDROID_GFX_JNI_REGISTRAR_H_
