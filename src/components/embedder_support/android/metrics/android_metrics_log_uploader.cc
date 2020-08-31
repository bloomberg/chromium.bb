// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/metrics/android_metrics_log_uploader.h"

#include "base/android/jni_array.h"
#include "components/embedder_support/android/metrics/jni/AndroidMetricsLogUploader_jni.h"
#include "components/metrics/log_decoder.h"

using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace metrics {

AndroidMetricsLogUploader::AndroidMetricsLogUploader(
    const MetricsLogUploader::UploadCallback& on_upload_complete)
    : on_upload_complete_(on_upload_complete) {}

AndroidMetricsLogUploader::~AndroidMetricsLogUploader() = default;

void AndroidMetricsLogUploader::UploadLog(
    const std::string& compressed_log_data,
    const std::string& /*log_hash*/,
    const std::string& /*log_signature*/,
    const ReportingInfo& reporting_info) {
  // This uploader uses the platform logging mechanism instead of the normal UMA
  // server. The platform mechanism does its own compression, so undo the
  // previous compression.
  std::string log_data;
  if (!DecodeLogData(compressed_log_data, &log_data)) {
    // If the log is corrupt, pretend the server rejected it (HTTP Bad Request).
    on_upload_complete_.Run(400, 0, true);
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> java_data = ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(log_data.data()), log_data.size());
  Java_AndroidMetricsLogUploader_uploadLog(env, java_data);

  // The platform mechanism doesn't provide a response code or any way to handle
  // failures, so we have nothing to pass to on_upload_complete. Just pass 200
  // (HTTP OK) with error code 0 and pretend everything is peachy.
  on_upload_complete_.Run(200, 0, true);
}

}  // namespace metrics
