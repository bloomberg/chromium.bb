// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/common/content_capture_features.h"
#include "jni/ContentCaptureFeatures_jni.h"

static jboolean JNI_ContentCaptureFeatures_IsEnabled(JNIEnv* env) {
  return content_capture::features::IsContentCaptureEnabled();
}
