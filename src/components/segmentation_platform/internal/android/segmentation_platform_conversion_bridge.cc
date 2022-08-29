// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/android/segmentation_platform_conversion_bridge.h"

#include "components/segmentation_platform/internal/jni_headers/SegmentationPlatformConversionBridge_jni.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/segmentation_platform/public/trigger_context.h"

namespace segmentation_platform {

// static
ScopedJavaLocalRef<jobject>
SegmentationPlatformConversionBridge::CreateJavaSegmentSelectionResult(
    JNIEnv* env,
    const SegmentSelectionResult& result) {
  int selected_segment = result.segment.has_value()
                             ? result.segment.value()
                             : proto::SegmentId::OPTIMIZATION_TARGET_UNKNOWN;
  return Java_SegmentationPlatformConversionBridge_createSegmentSelectionResult(
      env, result.is_ready, selected_segment);
}

// static
ScopedJavaLocalRef<jobject>
SegmentationPlatformConversionBridge::CreateJavaOnDemandSegmentSelectionResult(
    JNIEnv* env,
    const SegmentSelectionResult& result,
    const TriggerContext& trigger_context) {
  return Java_SegmentationPlatformConversionBridge_createOnDemandSegmentSelectionResult(
      env, CreateJavaSegmentSelectionResult(env, result),
      trigger_context.CreateJavaObject());
}

}  // namespace segmentation_platform
