// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "components/signin/public/android/jni_headers/CoreAccountId_jni.h"
#include "components/signin/public/android/jni_headers/CoreAccountInfo_jni.h"
#endif

namespace {

// Updates |field| with |new_value| if non-empty and different; if |new_value|
// is equal to |default_value| then it won't override |field| unless it is not
// set. Returns whether |field| was changed.
bool UpdateField(std::string* field,
                 const std::string& new_value,
                 const char* default_value) {
  if (*field == new_value || new_value.empty())
    return false;

  if (!field->empty() && default_value && new_value == default_value)
    return false;

  *field = new_value;
  return true;
}

// Updates |field| with |new_value| if true. Returns whether |field| was
// changed.
bool UpdateField(bool* field, bool new_value) {
  if (*field == new_value || !new_value)
    return false;

  *field = new_value;
  return true;
}

}  // namespace

// This must be a string which can never be a valid domain.
const char kNoHostedDomainFound[] = "NO_HOSTED_DOMAIN";

// This must be a string which can never be a valid picture URL.
const char kNoPictureURLFound[] = "NO_PICTURE_URL";

CoreAccountInfo::CoreAccountInfo() = default;

CoreAccountInfo::~CoreAccountInfo() = default;

CoreAccountInfo::CoreAccountInfo(const CoreAccountInfo& other) = default;

CoreAccountInfo::CoreAccountInfo(CoreAccountInfo&& other) noexcept = default;

CoreAccountInfo& CoreAccountInfo::operator=(const CoreAccountInfo& other) =
    default;

CoreAccountInfo& CoreAccountInfo::operator=(CoreAccountInfo&& other) noexcept =
    default;

bool CoreAccountInfo::IsEmpty() const {
  return account_id.empty() && email.empty() && gaia.empty();
}

AccountInfo::AccountInfo() = default;

AccountInfo::~AccountInfo() = default;

AccountInfo::AccountInfo(const AccountInfo& other) = default;

AccountInfo::AccountInfo(AccountInfo&& other) noexcept = default;

AccountInfo& AccountInfo::operator=(const AccountInfo& other) = default;

AccountInfo& AccountInfo::operator=(AccountInfo&& other) noexcept = default;

bool AccountInfo::IsEmpty() const {
  return CoreAccountInfo::IsEmpty() && hosted_domain.empty() &&
         full_name.empty() && given_name.empty() && locale.empty() &&
         picture_url.empty();
}

bool AccountInfo::IsValid() const {
  return !account_id.empty() && !email.empty() && !gaia.empty() &&
         !hosted_domain.empty() && !full_name.empty() && !given_name.empty() &&
         !locale.empty() && !picture_url.empty();
}

bool AccountInfo::UpdateWith(const AccountInfo& other) {
  if (account_id != other.account_id) {
    // Only updates with a compatible AccountInfo.
    return false;
  }

  bool modified = false;
  modified |= UpdateField(&gaia, other.gaia, nullptr);
  modified |= UpdateField(&email, other.email, nullptr);
  modified |= UpdateField(&full_name, other.full_name, nullptr);
  modified |= UpdateField(&given_name, other.given_name, nullptr);
  modified |=
      UpdateField(&hosted_domain, other.hosted_domain, kNoHostedDomainFound);
  modified |= UpdateField(&locale, other.locale, nullptr);
  modified |= UpdateField(&picture_url, other.picture_url, kNoPictureURLFound);
  modified |= UpdateField(&is_child_account, other.is_child_account);
  modified |= UpdateField(&is_under_advanced_protection,
                          other.is_under_advanced_protection);

  return modified;
}
bool operator==(const CoreAccountInfo& l, const CoreAccountInfo& r) {
  return l.account_id == r.account_id && l.gaia == r.gaia &&
         gaia::AreEmailsSame(l.email, r.email) &&
         l.is_under_advanced_protection == r.is_under_advanced_protection;
}
bool operator!=(const CoreAccountInfo& l, const CoreAccountInfo& r) {
  return !(l == r);
}

std::ostream& operator<<(std::ostream& os, const CoreAccountInfo& account) {
  os << "account_id: " << account.account_id << ", gaia: " << account.gaia
     << ", email: " << account.email << ", adv_prot: " << std::boolalpha
     << account.is_under_advanced_protection;
  return os;
}

#if defined(OS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaCoreAccountInfo(
    JNIEnv* env,
    const CoreAccountInfo& account_info) {
  return signin::Java_CoreAccountInfo_Constructor(
      env, ConvertToJavaCoreAccountId(env, account_info.account_id),
      base::android::ConvertUTF8ToJavaString(env, account_info.email),
      base::android::ConvertUTF8ToJavaString(env, account_info.gaia));
}

base::android::ScopedJavaLocalRef<jobject> ConvertToJavaCoreAccountId(
    JNIEnv* env,
    const CoreAccountId& account_id) {
  DCHECK(!account_id.empty());
  return signin::Java_CoreAccountId_Constructor(
      env, base::android::ConvertUTF8ToJavaString(env, account_id.ToString()));
}

CoreAccountInfo ConvertFromJavaCoreAccountInfo(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_core_account_info) {
  CoreAccountInfo account;
  account.account_id = ConvertFromJavaCoreAccountId(
      env, signin::Java_CoreAccountInfo_getId(env, j_core_account_info));
  account.gaia = base::android::ConvertJavaStringToUTF8(
      signin::Java_CoreAccountInfo_getGaiaId(env, j_core_account_info));
  account.email = base::android::ConvertJavaStringToUTF8(
      signin::Java_CoreAccountInfo_getEmail(env, j_core_account_info));
  return account;
}

CoreAccountId ConvertFromJavaCoreAccountId(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_core_account_id) {
  CoreAccountId id =
      CoreAccountId::FromString(base::android::ConvertJavaStringToUTF8(
          signin::Java_CoreAccountId_getId(env, j_core_account_id)));
  return id;
}
#endif
