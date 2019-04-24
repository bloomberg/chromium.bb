// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/send_tab_to_self_android_bridge.h"

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "jni/SendTabToSelfAndroidBridge_jni.h"
#include "jni/SendTabToSelfEntry_jni.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace send_tab_to_self {

namespace {

ScopedJavaLocalRef<jobject> CreateJavaSendTabToSelfEntry(
    JNIEnv* env,
    const SendTabToSelfEntry* entry) {
  return Java_SendTabToSelfEntry_createSendTabToSelfEntry(
      env, ConvertUTF8ToJavaString(env, entry->GetGUID()),
      ConvertUTF8ToJavaString(env, entry->GetURL().spec()),
      ConvertUTF8ToJavaString(env, entry->GetTitle()),
      entry->GetSharedTime().ToJavaTime(),
      entry->GetOriginalNavigationTime().ToJavaTime(),
      ConvertUTF8ToJavaString(env, entry->GetDeviceName()));
}

}  // namespace

SendTabToSelfAndroidBridge::SendTabToSelfAndroidBridge(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jobject>& j_profile)
    : send_tab_to_self_model_(nullptr), weak_java_ref_(env, obj) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  send_tab_to_self_model_ = SendTabToSelfSyncServiceFactory::GetInstance()
                                ->GetForProfile(profile)
                                ->GetSendTabToSelfModel();
}

static jlong JNI_SendTabToSelfAndroidBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_profile) {
  SendTabToSelfAndroidBridge* send_tab_to_self_android_bridge =
      new SendTabToSelfAndroidBridge(env, obj, j_profile);
  return reinterpret_cast<intptr_t>(send_tab_to_self_android_bridge);
}

void SendTabToSelfAndroidBridge::Destroy(JNIEnv*,
                                         const JavaParamRef<jobject>&) {
  delete this;
}

void SendTabToSelfAndroidBridge::GetAllGuids(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_guid_list_obj) {
  // TODO(tgupta): Check that the model is loaded
  // if (!send_tab_to_self_model_->loaded()())
  //   return;

  std::vector<std::string> all_ids = send_tab_to_self_model_->GetAllGuids();
  for (std::vector<std::string>::iterator it = all_ids.begin();
       it != all_ids.end(); ++it) {
    ScopedJavaLocalRef<jstring> j_guid = ConvertUTF8ToJavaString(env, *it);
    Java_SendTabToSelfAndroidBridge_addToGuidList(env, j_guid_list_obj, j_guid);
  }
}

void SendTabToSelfAndroidBridge::DeleteAllEntries(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  send_tab_to_self_model_->DeleteAllEntries();
}

ScopedJavaLocalRef<jobject> SendTabToSelfAndroidBridge::AddEntry(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jstring>& j_title) {
  const std::string url = ConvertJavaStringToUTF8(env, j_url);
  const std::string title = ConvertJavaStringToUTF8(env, j_title);

  const SendTabToSelfEntry* persisted_entry =
      send_tab_to_self_model_->AddEntry(GURL(url), title);

  if (persisted_entry == nullptr) {
    return nullptr;
  }

  return CreateJavaSendTabToSelfEntry(env, persisted_entry);
}

ScopedJavaLocalRef<jobject> SendTabToSelfAndroidBridge::GetEntryByGUID(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_guid) {
  const std::string guid = ConvertJavaStringToUTF8(env, j_guid);
  const SendTabToSelfEntry* found_entry =
      send_tab_to_self_model_->GetEntryByGUID(guid);

  if (found_entry == nullptr) {
    return nullptr;
  }

  return CreateJavaSendTabToSelfEntry(env, found_entry);
}

}  // namespace send_tab_to_self
