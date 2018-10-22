// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_image_loader_bridge.h"

#include <jni.h>

#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/feed/feed_host_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feed/content/feed_host_service.h"
#include "components/feed/core/feed_image_manager.h"
#include "jni/FeedImageLoaderBridge_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::JavaIntArrayToIntVector;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace feed {

static jlong JNI_FeedImageLoaderBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_this,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  FeedHostService* host_service =
      FeedHostServiceFactory::GetForBrowserContext(profile);
  FeedImageManager* feed_image_manager = host_service->GetImageManager();
  DCHECK(feed_image_manager);
  FeedImageLoaderBridge* native_image_loader_bridge =
      new FeedImageLoaderBridge(feed_image_manager);
  return reinterpret_cast<intptr_t>(native_image_loader_bridge);
}

FeedImageLoaderBridge::FeedImageLoaderBridge(
    FeedImageManager* feed_image_manager)
    : feed_image_manager_(feed_image_manager), weak_ptr_factory_(this) {}

FeedImageLoaderBridge::~FeedImageLoaderBridge() = default;

void FeedImageLoaderBridge::Destroy(JNIEnv* env,
                                    const JavaRef<jobject>& j_this) {
  delete this;
}

void FeedImageLoaderBridge::FetchImage(JNIEnv* j_env,
                                       const JavaRef<jobject>& j_this,
                                       const JavaRef<jobjectArray>& j_urls,
                                       const JavaRef<jobject>& j_callback) {
  std::vector<std::string> urls;
  base::android::AppendJavaStringArrayToStringVector(j_env, j_urls.obj(),
                                                     &urls);

  ScopedJavaGlobalRef<jobject> callback(j_callback);
  feed_image_manager_->FetchImage(
      urls, base::BindOnce(&FeedImageLoaderBridge::OnImageFetched,
                           weak_ptr_factory_.GetWeakPtr(), callback));
}

void FeedImageLoaderBridge::OnImageFetched(
    ScopedJavaGlobalRef<jobject> callback,
    const gfx::Image& image) {
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!image.IsEmpty()) {
    j_bitmap = gfx::ConvertToJavaBitmap(image.ToSkBitmap());
  }
  RunObjectCallbackAndroid(callback, j_bitmap);
}

}  // namespace feed
