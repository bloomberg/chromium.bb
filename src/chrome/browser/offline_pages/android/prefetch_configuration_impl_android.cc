// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_configuration_impl.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "jni/PrefetchConfiguration_jni.h"

using base::android::JavaParamRef;

// These functions fulfill the Java to native link between
// PrefetchConfiguration.java and PrefetchConfigurationImpl.

namespace offline_pages {
namespace android {

namespace {

PrefetchConfigurationImpl* PrefetchConfigurationImplFromJProfile(
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  if (!profile)
    return nullptr;

  PrefetchService* prefetch_service =
      PrefetchServiceFactory::GetForBrowserContext(profile);
  if (!prefetch_service)
    return nullptr;

  // The cast below is safe because PrefetchConfigurationImpl is Chrome's
  // implementation of PrefetchConfiguration.
  return static_cast<PrefetchConfigurationImpl*>(
      prefetch_service->GetPrefetchConfiguration());
}

}  // namespace

JNI_EXPORT jboolean JNI_PrefetchConfiguration_IsPrefetchingEnabled(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jprofile) {
  PrefetchConfigurationImpl* config_impl =
      PrefetchConfigurationImplFromJProfile(jprofile);
  if (config_impl)
    return static_cast<jboolean>(config_impl->IsPrefetchingEnabled());
  return false;
}

JNI_EXPORT void JNI_PrefetchConfiguration_SetPrefetchingEnabledInSettings(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jprofile,
    jboolean enabled) {
  PrefetchConfigurationImpl* config_impl =
      PrefetchConfigurationImplFromJProfile(jprofile);
  if (config_impl)
    config_impl->SetPrefetchingEnabledInSettings(enabled);
}

}  // namespace android
}  // namespace offline_pages
