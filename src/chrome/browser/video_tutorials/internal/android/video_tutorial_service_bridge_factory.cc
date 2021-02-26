// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/webui/video_tutorials/video_player_source.h"
#include "chrome/browser/video_tutorials/internal/android/video_tutorial_service_bridge.h"
#include "chrome/browser/video_tutorials/internal/jni_headers/VideoTutorialServiceFactory_jni.h"
#include "chrome/browser/video_tutorials/video_tutorial_service_factory.h"

namespace {

void InitializeWebUIDataSource(Profile* profile) {
  static bool s_initialized_web_ui_data_source = false;
  if (!s_initialized_web_ui_data_source) {
    content::WebUIDataSource::Add(
        profile, video_tutorials::CreateVideoPlayerUntrustedDataSource());
    s_initialized_web_ui_data_source = true;
  }
}

}  // namespace

// Takes a Java Profile and returns a Java VideoTutorialService.
static base::android::ScopedJavaLocalRef<jobject>
JNI_VideoTutorialServiceFactory_GetForProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  ProfileKey* profile_key = profile->GetProfileKey();

  // TODO(shaktisahu): Move this to VideoTutorialServiceFactory.
  InitializeWebUIDataSource(profile);

  // Return null if there is no reasonable context for the provided Java
  // profile.
  if (profile_key == nullptr)
    return base::android::ScopedJavaLocalRef<jobject>();

  video_tutorials::VideoTutorialService* tile_service =
      video_tutorials::VideoTutorialServiceFactory::GetInstance()->GetForKey(
          profile_key);
  return video_tutorials::VideoTutorialServiceBridge::
      GetBridgeForVideoTutorialService(tile_service);
}
