// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_MEDIA_PARSER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_MEDIA_PARSER_BRIDGE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"

class DownloadMediaParser;

// The JNI bridge that uses DownloadMediaParser to parse local media file. The
// bridge is owned by the Java side.
class DownloadMediaParserBridge {
 public:
  DownloadMediaParserBridge();
  ~DownloadMediaParserBridge();

  void Destory(JNIEnv* env, jobject obj);
  void ParseMediaFile(JNIEnv* env,
                      jobject obj,
                      const base::android::JavaParamRef<jstring>& jmime_type,
                      const base::android::JavaParamRef<jstring>& jfile_path,
                      jlong jtotal_size,
                      const base::android::JavaParamRef<jobject>& jcallback);

 private:
  // The task runner that performs blocking disk IO. Created by this object.
  scoped_refptr<base::SingleThreadTaskRunner> disk_io_task_runner_;

  // The media parser that does actual jobs in a sandboxed process.
  std::unique_ptr<DownloadMediaParser> parser_;

  DISALLOW_COPY_AND_ASSIGN(DownloadMediaParserBridge);
};

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_MEDIA_PARSER_BRIDGE_H_
