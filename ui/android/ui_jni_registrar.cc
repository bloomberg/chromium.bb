// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/ui_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "ui/gfx/android/window_android.h"
#include "ui/shell_dialogs/select_file_dialog_android.h"

namespace ui {

static base::android::RegistrationMethod kUiRegisteredMethods[] = {
  { "NativeWindow", WindowAndroid::RegisterWindowAndroid },
  { "SelectFileDialog", SelectFileDialogImpl::RegisterSelectFileDialog },
};

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kUiRegisteredMethods,
                               arraysize(kUiRegisteredMethods));
}

} // namespace ui
