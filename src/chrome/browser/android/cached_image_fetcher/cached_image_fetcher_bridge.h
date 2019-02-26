// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CACHED_IMAGE_FETCHER_CACHED_IMAGE_FETCHER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_CACHED_IMAGE_FETCHER_CACHED_IMAGE_FETCHER_BRIDGE_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "ui/gfx/image/image.h"

namespace image_fetcher {

class CachedImageFetcher;

// Native counterpart of CachedImageFetcherBridge.java. Owns an instance of
// CachedImageFetcher.
class CachedImageFetcherBridge {
 public:
  explicit CachedImageFetcherBridge(
      std::unique_ptr<CachedImageFetcher> cached_image_fetcher,
      base::FilePath base_file_path);
  ~CachedImageFetcherBridge();

  void Destroy(JNIEnv* j_env, const base::android::JavaRef<jobject>& j_this);

  base::android::ScopedJavaLocalRef<jstring> GetFilePath(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this,
      const base::android::JavaRef<jstring>& j_url);

  void FetchImage(JNIEnv* j_env,
                  const base::android::JavaRef<jobject>& j_this,
                  const base::android::JavaRef<jstring>& j_url,
                  const jint width_px,
                  const jint height_px,
                  const base::android::JavaRef<jobject>& j_callback);

 private:
  void OnImageFetched(base::android::ScopedJavaGlobalRef<jobject> callback,
                      const std::string& id,
                      const gfx::Image& image,
                      const RequestMetadata& request_metadata);

  std::unique_ptr<CachedImageFetcher> cached_image_fetcher_;
  base::FilePath base_file_path_;

  base::WeakPtrFactory<CachedImageFetcherBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CachedImageFetcherBridge);
};

}  // namespace image_fetcher

#endif  // CHROME_BROWSER_ANDROID_CACHED_IMAGE_FETCHER_CACHED_IMAGE_FETCHER_BRIDGE_H_
