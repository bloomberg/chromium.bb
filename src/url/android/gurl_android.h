// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_ANDROID_GURL_ANDROID_H_
#define URL_ANDROID_GURL_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "url/gurl.h"

namespace url {

class GURLAndroid {
 public:
  static std::unique_ptr<GURL> ToNativeGURL(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_gurl);
  static base::android::ScopedJavaLocalRef<jobject> FromNativeGURL(
      JNIEnv* env,
      const GURL& gurl);
  static base::android::ScopedJavaLocalRef<jobject> EmptyGURL(JNIEnv* env);
};

}  // namespace url

#endif  // URL_ANDROID_GURL_ANDROID_H_
