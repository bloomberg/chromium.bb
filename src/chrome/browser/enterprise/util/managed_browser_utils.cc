// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/managed_browser_utils.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"

#if defined(OS_ANDROID)
#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/enterprise/util/jni_headers/ManagedBrowserUtils_jni.h"
#include "chrome/browser/profiles/profile_android.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user_manager.h"
#endif  // defined(OS_CHROMEOS)

namespace chrome {
namespace enterprise_util {

bool HasBrowserPoliciesApplied(Profile* profile) {
  // This profile may have policies configured.
  auto* profile_connector = profile->GetProfilePolicyConnector();
  if (profile_connector->IsManaged())
    return true;

#if defined(OS_CHROMEOS)
  // This session's primary user may also have policies, and those policies may
  // not have per-profile support.
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (primary_user) {
    auto* primary_profile =
        chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
    if (primary_profile) {
      auto* primary_profile_connector =
          primary_profile->GetProfilePolicyConnector();
      if (primary_profile_connector->IsManaged())
        return true;
    }
  }

  // The machine may be enrolled, via Google Cloud or Active Directory.
  auto* browser_connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (browser_connector->IsEnterpriseManaged())
    return true;
#else
  // There may be policies set in a platform-specific way (e.g. Windows
  // Registry), or with machine level user cloud policies.
  auto* browser_connector = g_browser_process->browser_policy_connector();
  if (browser_connector->HasMachineLevelPolicies())
    return true;
#endif

  return false;
}

}  // namespace enterprise_util
}  // namespace chrome

#if defined(OS_ANDROID)

// static
jboolean JNI_ManagedBrowserUtils_HasBrowserPoliciesApplied(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& profile) {
  return chrome::enterprise_util::HasBrowserPoliciesApplied(
      ProfileAndroid::FromProfileAndroid(profile));
}

#endif  // defined(OS_ANDROID)
