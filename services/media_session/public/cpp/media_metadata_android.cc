// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_metadata.h"

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "jni/MediaMetadata_jni.h"

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

base::android::ScopedJavaLocalRef<jobject> MediaMetadata::CreateJavaObject(
    JNIEnv* env) {
  ScopedJavaLocalRef<jstring> j_title(
      base::android::ConvertUTF16ToJavaString(env, title));
  ScopedJavaLocalRef<jstring> j_artist(
      base::android::ConvertUTF16ToJavaString(env, artist));
  ScopedJavaLocalRef<jstring> j_album(
      base::android::ConvertUTF16ToJavaString(env, album));

  ScopedJavaLocalRef<jobject> j_metadata =
      Java_MediaMetadata_create(env, j_title, j_artist, j_album);

  for (const auto& image : artwork) {
    std::string src = image.src.spec();
    ScopedJavaLocalRef<jstring> j_src(
        base::android::ConvertUTF8ToJavaString(env, src));
    ScopedJavaLocalRef<jstring> j_type(
        base::android::ConvertUTF16ToJavaString(env, image.type));
    ScopedJavaLocalRef<jintArray> j_sizes(
        base::android::ToJavaIntArray(env, GetFlattenedSizeArray(image.sizes)));

    Java_MediaMetadata_createAndAddMediaImage(env, j_metadata, j_src, j_type,
                                              j_sizes);
  }

  return j_metadata;
}

}  // namespace media_session
