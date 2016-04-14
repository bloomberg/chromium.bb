// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/android/url_jni_registrar.h"
#include "url/url_features.h"

#if BUILDFLAG(USE_PLATFORM_ICU_ALTERNATIVES)
#include "url/url_canon_icu_alternatives_android.h"
#endif

namespace url {
namespace android {

bool RegisterJni(JNIEnv* env) {
#if BUILDFLAG(USE_PLATFORM_ICU_ALTERNATIVES)
  return RegisterIcuAlternativesJni(env);
#endif

  // Do nothing if USE_PLATFORM_ICU_ALTERNATIVES is false.
  return true;
}

}  // namespace android
}  // namespace url
