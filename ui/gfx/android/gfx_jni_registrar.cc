// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/gfx_jni_registrar.h"

namespace gfx {

void RegisterBitmapAndroid(JNIEnv* env);

bool RegisterJni(JNIEnv* env) {
  RegisterBitmapAndroid(env);
  return true;
}

}  // namespace gfx
