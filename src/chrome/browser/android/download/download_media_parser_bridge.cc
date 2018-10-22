// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/download_media_parser_bridge.h"

#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/task/post_task.h"
#include "chrome/browser/android/download/download_media_parser.h"
#include "jni/DownloadMediaParserBridge_jni.h"

// static
jlong JNI_DownloadMediaParserBridge_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  auto* bridge = new DownloadMediaParserBridge;
  return reinterpret_cast<intptr_t>(bridge);
}

DownloadMediaParserBridge::DownloadMediaParserBridge()
    : disk_io_task_runner_(
          base::CreateSingleThreadTaskRunnerWithTraits({base::MayBlock()})),
      parser_(std::make_unique<DownloadMediaParser>(disk_io_task_runner_)) {}

DownloadMediaParserBridge::~DownloadMediaParserBridge() = default;

void DownloadMediaParserBridge::Destory(JNIEnv* env, jobject obj) {
  delete this;
}

void DownloadMediaParserBridge::ParseMediaFile(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jstring>& jmime_type,
    const base::android::JavaParamRef<jstring>& jfile_path,
    jlong jtotal_size,
    const base::android::JavaParamRef<jobject>& jcallback) {
  base::FilePath file_path(
      base::android::ConvertJavaStringToUTF8(env, jfile_path));
  std::string mime_type =
      base::android::ConvertJavaStringToUTF8(env, jmime_type);

  // TODO(xingliu): Pass the result back to Java.
  parser_->ParseMediaFile(static_cast<int64_t>(jtotal_size), mime_type,
                          file_path, base::DoNothing());
}
