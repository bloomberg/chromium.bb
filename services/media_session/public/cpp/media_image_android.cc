// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_image.h"

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "jni/MediaImage_jni.h"

using base::android::ScopedJavaLocalRef;

namespace media_session {

namespace {

std::vector<int> GetFlattenedSizeArray(const std::vector<gfx::Size>& sizes) {
  std::vector<int> flattened_array;
  flattened_array.reserve(2 * sizes.size());
  for (const auto& size : sizes) {
    flattened_array.push_back(size.width());
    flattened_array.push_back(size.height());
  }
  return flattened_array;
}

}  // anonymous namespace

base::android::ScopedJavaLocalRef<jobject> MediaImage::CreateJavaObject(
    JNIEnv* env) const {
  std::string src_spec = src.spec();
  ScopedJavaLocalRef<jstring> j_src(
      base::android::ConvertUTF8ToJavaString(env, src_spec));
  ScopedJavaLocalRef<jstring> j_type(
      base::android::ConvertUTF16ToJavaString(env, type));
  ScopedJavaLocalRef<jintArray> j_sizes(
      base::android::ToJavaIntArray(env, GetFlattenedSizeArray(sizes)));

  return Java_MediaImage_create(env, j_src, j_type, j_sizes);
}

}  // namespace media_session
