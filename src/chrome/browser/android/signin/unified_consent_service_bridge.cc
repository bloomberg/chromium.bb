// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "jni/UnifiedConsentServiceBridge_jni.h"

using base::android::JavaParamRef;

static jboolean JNI_UnifiedConsentServiceBridge_IsUnifiedConsentGiven(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& profileAndroid) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(profileAndroid);
  auto* unifiedConsentService =
      UnifiedConsentServiceFactory::GetForProfile(profile);
  return unifiedConsentService->IsUnifiedConsentGiven();
}

static void JNI_UnifiedConsentServiceBridge_SetUnifiedConsentGiven(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& profileAndroid,
    jboolean unifiedConsentGiven) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(profileAndroid);
  auto* unifiedConsentService =
      UnifiedConsentServiceFactory::GetForProfile(profile);
  unifiedConsentService->SetUnifiedConsentGiven(unifiedConsentGiven);
}

static jboolean JNI_UnifiedConsentServiceBridge_ShouldShowConsentBump(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& profileAndroid) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(profileAndroid);
  auto* unifiedConsentService =
      UnifiedConsentServiceFactory::GetForProfile(profile);
  return unifiedConsentService->ShouldShowConsentBump();
}

static jboolean
JNI_UnifiedConsentServiceBridge_IsUrlKeyedAnonymizedDataCollectionEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& profileAndroid) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(profileAndroid);
  return profile->GetPrefs()->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}

static void
JNI_UnifiedConsentServiceBridge_SetUrlKeyedAnonymizedDataCollectionEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& profileAndroid,
    const jboolean enabled) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(profileAndroid);
  profile->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      enabled);
}

static jboolean
JNI_UnifiedConsentServiceBridge_IsUrlKeyedAnonymizedDataCollectionManaged(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& profileAndroid) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(profileAndroid);
  return profile->GetPrefs()->IsManagedPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}
