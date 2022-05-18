// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_check/android/password_check_bridge.h"

#include <jni.h>
#include <string>

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_check/android/jni_headers/CompromisedCredential_jni.h"
#include "chrome/browser/password_check/android/jni_headers/PasswordCheckBridge_jni.h"
#include "chrome/browser/password_manager/android/password_checkup_launcher_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/password_manager/content/browser/password_change_success_tracker_factory.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "url/android/gurl_android.h"

using password_manager::PasswordChangeSuccessTracker;

namespace {

password_manager::CredentialView ConvertJavaObjectToCredentialView(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential) {
  std::string signon_realm = ConvertJavaStringToUTF8(
      env, Java_CompromisedCredential_getSignonRealm(env, credential));
  password_manager::FacetURI facet =
      password_manager::FacetURI::FromPotentiallyInvalidSpec(signon_realm);
  // For the UI, Android credentials store the affiliated realm in the
  // url field, however the saved credential should contains the signon realm
  // instead.
  GURL url = facet.IsValidAndroidFacetURI()
                 ? GURL(signon_realm)
                 : *url::GURLAndroid::ToNativeGURL(
                       env, Java_CompromisedCredential_getAssociatedUrl(
                                env, credential));
  return password_manager::CredentialView(
      std::move(signon_realm), std::move(url),
      ConvertJavaStringToUTF16(
          env, Java_CompromisedCredential_getUsername(env, credential)),
      ConvertJavaStringToUTF16(
          env, Java_CompromisedCredential_getPassword(env, credential)),
      base::Time::FromJavaTime(
          Java_CompromisedCredential_getLastUsedTime(env, credential)));
}

// Checks whether the credential is leaked but not phished. Other compromising
// states are ignored (e.g. weak or reused).
constexpr bool IsOnlyLeaked(
    const password_manager::InsecureCredentialTypeFlags& flags) {
  using password_manager::InsecureCredentialTypeFlags;
  if ((flags & InsecureCredentialTypeFlags::kCredentialPhished) !=
      InsecureCredentialTypeFlags::kSecure) {
    return false;
  }
  return (flags & InsecureCredentialTypeFlags::kCredentialLeaked) !=
         InsecureCredentialTypeFlags::kSecure;
}

// Checks whether the credential is phished but not leaked. Other compromising
// states are ignored (e.g. weak or reused).
constexpr bool IsOnlyPhished(
    const password_manager::InsecureCredentialTypeFlags& flags) {
  using password_manager::InsecureCredentialTypeFlags;
  if ((flags & InsecureCredentialTypeFlags::kCredentialLeaked) !=
      InsecureCredentialTypeFlags::kSecure) {
    return false;
  }
  return (flags & InsecureCredentialTypeFlags::kCredentialPhished) !=
         InsecureCredentialTypeFlags::kSecure;
}

}  // namespace

static jlong JNI_PasswordCheckBridge_Create(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_bridge) {
  return reinterpret_cast<intptr_t>(new PasswordCheckBridge(java_bridge));
}

PasswordCheckBridge::PasswordCheckBridge(
    const base::android::JavaParamRef<jobject>& java_bridge)
    : java_bridge_(java_bridge) {}

PasswordCheckBridge::~PasswordCheckBridge() = default;

void PasswordCheckBridge::StartCheck(JNIEnv* env) {
  check_manager_.StartCheck();
}

void PasswordCheckBridge::StopCheck(JNIEnv* env) {
  check_manager_.StopCheck();
}

int64_t PasswordCheckBridge::GetLastCheckTimestamp(JNIEnv* env) {
  return check_manager_.GetLastCheckTimestamp().ToJavaTime();
}

jint PasswordCheckBridge::GetCompromisedCredentialsCount(JNIEnv* env) {
  return check_manager_.GetCompromisedCredentialsCount();
}

jint PasswordCheckBridge::GetSavedPasswordsCount(JNIEnv* env) {
  return check_manager_.GetSavedPasswordsCount();
}

void PasswordCheckBridge::GetCompromisedCredentials(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& java_credentials) {
  std::vector<PasswordCheckManager::CompromisedCredentialForUI> credentials =
      check_manager_.GetCompromisedCredentials();

  for (size_t i = 0; i < credentials.size(); ++i) {
    const auto& credential = credentials[i];
    Java_PasswordCheckBridge_insertCredential(
        env, java_credentials, i,
        base::android::ConvertUTF8ToJavaString(env, credential.signon_realm),
        url::GURLAndroid::FromNativeGURL(env, credential.url),
        base::android::ConvertUTF16ToJavaString(env, credential.username),
        base::android::ConvertUTF16ToJavaString(env, credential.display_origin),
        base::android::ConvertUTF16ToJavaString(env,
                                                credential.display_username),
        base::android::ConvertUTF16ToJavaString(env, credential.password),
        base::android::ConvertUTF8ToJavaString(env,
                                               credential.change_password_url),
        base::android::ConvertUTF8ToJavaString(env, credential.package_name),
        credential.create_time.ToJavaTime(),
        credential.last_used_time.ToJavaTime(),
        IsOnlyLeaked(credential.insecure_type),
        IsOnlyPhished(credential.insecure_type),
        credential.has_startable_script, credential.has_auto_change_button);
  }
}

void PasswordCheckBridge::LaunchCheckupInAccount(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& activity) {
  PasswordCheckupLauncherHelper::LaunchCheckupInAccountWithActivity(
      env,
      base::android::ConvertUTF8ToJavaString(
          env, password_manager::GetPasswordCheckupURL().spec()),
      activity);
}

void PasswordCheckBridge::UpdateCredential(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential,
    const base::android::JavaParamRef<jstring>& new_password) {
  check_manager_.UpdateCredential(
      ConvertJavaObjectToCredentialView(env, credential),
      base::android::ConvertJavaStringToUTF8(new_password));
}

void PasswordCheckBridge::OnEditCredential(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential,
    const base::android::JavaParamRef<jobject>& context,
    const base::android::JavaParamRef<jobject>& settings_launcher) {
  check_manager_.OnEditCredential(
      ConvertJavaObjectToCredentialView(env, credential), context,
      settings_launcher);
}

void PasswordCheckBridge::RemoveCredential(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential) {
  check_manager_.RemoveCredential(
      ConvertJavaObjectToCredentialView(env, credential));
}

void PasswordCheckBridge::Destroy(JNIEnv* env) {
  check_manager_.StopCheck();
  delete this;
}

bool PasswordCheckBridge::AreScriptsRefreshed(JNIEnv* env) const {
  return check_manager_.AreScriptsRefreshed();
}

void PasswordCheckBridge::RefreshScripts(JNIEnv* env) {
  check_manager_.RefreshScripts();
}

void PasswordCheckBridge::OnSavedPasswordsFetched(int count) {
  Java_PasswordCheckBridge_onSavedPasswordsFetched(
      base::android::AttachCurrentThread(), java_bridge_, count);
}

void PasswordCheckBridge::OnCompromisedCredentialsChanged(int count) {
  Java_PasswordCheckBridge_onCompromisedCredentialsFetched(
      base::android::AttachCurrentThread(), java_bridge_, count);
}

void PasswordCheckBridge::OnPasswordCheckStatusChanged(
    password_manager::PasswordCheckUIStatus status) {
  Java_PasswordCheckBridge_onPasswordCheckStatusChanged(
      base::android::AttachCurrentThread(), java_bridge_,
      static_cast<int>(status));
}

void PasswordCheckBridge::OnPasswordCheckProgressChanged(
    int already_processed,
    int remaining_in_queue) {
  Java_PasswordCheckBridge_onPasswordCheckProgressChanged(
      base::android::AttachCurrentThread(), java_bridge_, already_processed,
      remaining_in_queue);
}

void PasswordCheckBridge::OnAutomatedPasswordChangeStarted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential) {
  password_manager::CredentialView credential_view =
      ConvertJavaObjectToCredentialView(env, credential);
  GetPasswordChangeSuccessTracker()->OnChangePasswordFlowStarted(
      credential_view.url, base::UTF16ToUTF8(credential_view.username),
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);
}

void PasswordCheckBridge::OnManualPasswordChangeStarted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential) {
  password_manager::CredentialView credential_view =
      ConvertJavaObjectToCredentialView(env, credential);
  GetPasswordChangeSuccessTracker()->OnManualChangePasswordFlowStarted(
      credential_view.url, base::UTF16ToUTF8(credential_view.username),
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);
}

password_manager::PasswordChangeSuccessTracker*
PasswordCheckBridge::GetPasswordChangeSuccessTracker() {
  return password_manager::PasswordChangeSuccessTrackerFactory::GetInstance()
      ->GetForBrowserContext(ProfileManager::GetLastUsedProfile());
}
