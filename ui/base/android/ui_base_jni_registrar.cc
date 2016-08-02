// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/android/ui_base_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "ui/base/l10n/l10n_util_android.h"

namespace ui {
namespace android {

static base::android::RegistrationMethod kUiRegisteredMethods[] = {
  { "LocalizationUtils", l10n_util::RegisterLocalizationUtil },
};

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kUiRegisteredMethods,
                               arraysize(kUiRegisteredMethods));
}

}  // namespace android
}  // namespace ui
