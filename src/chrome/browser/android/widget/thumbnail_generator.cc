// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/widget/thumbnail_generator.h"

#include <memory>

#include "base/android/jni_string.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_thread.h"
#include "jni/ThumbnailGenerator_jni.h"
#include "ui/gfx/android/java_bitmap.h"

class SkBitmap;

using base::android::JavaParamRef;

ThumbnailGenerator::ThumbnailGenerator(const JavaParamRef<jobject>& jobj)
    : java_delegate_(jobj), weak_factory_(this) {
  DCHECK(!jobj.is_null());
}

ThumbnailGenerator::~ThumbnailGenerator() = default;

void ThumbnailGenerator::Destroy(JNIEnv* env,
                                 const JavaParamRef<jobject>& jobj) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delete this;
}

void ThumbnailGenerator::OnThumbnailRetrieved(
    const base::android::ScopedJavaGlobalRef<jstring>& content_id,
    int icon_size,
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    const SkBitmap& thumbnail) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Send the bitmap back to Java-land.
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ThumbnailGenerator_onThumbnailRetrieved(
      env, java_delegate_, content_id, icon_size,
      thumbnail.drawsNothing() ? NULL : gfx::ConvertToJavaBitmap(&thumbnail),
      callback);
}

void ThumbnailGenerator::RetrieveThumbnail(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jstring>& jcontent_id,
    const JavaParamRef<jstring>& jfile_path,
    jint icon_size,
    const JavaParamRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string file_path =
      base::android::ConvertJavaStringToUTF8(env, jfile_path);

  auto request = std::make_unique<ImageThumbnailRequest>(
      icon_size,
      base::BindOnce(
          &ThumbnailGenerator::OnThumbnailRetrieved, weak_factory_.GetWeakPtr(),
          base::android::ScopedJavaGlobalRef<jstring>(jcontent_id), icon_size,
          base::android::ScopedJavaGlobalRef<jobject>(callback)));
  request->Start(base::FilePath::FromUTF8Unsafe(file_path));

  // Dropping ownership of |request| here because it will clean itself up once
  // the started request finishes.
  request.release();
}

// static
static jlong JNI_ThumbnailGenerator_Init(JNIEnv* env,
                                         const JavaParamRef<jobject>& jobj) {
  return reinterpret_cast<intptr_t>(new ThumbnailGenerator(jobj));
}
