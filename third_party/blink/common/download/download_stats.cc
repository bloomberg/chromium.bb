// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/download/download_stats.h"

#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace blink {

// static
unsigned DownloadStats::ToUmaValue(const DownloadFlags& flags) {
  unsigned value = 0;
  if (flags.has_sandbox)
    value |= kSandboxBit;
  if (flags.is_cross_origin)
    value |= kCrossOriginBit;
  if (flags.is_ad_frame)
    value |= kAdBit;
  if (flags.has_gesture)
    value |= kGestureBit;
  return value;
}

// static
void DownloadStats::RecordMainFrameHasGesture(bool has_gesture,
                                              ukm::SourceId source_id,
                                              ukm::UkmRecorder* ukm_recorder) {
  UMA_HISTOGRAM_BOOLEAN("Download.MainFrame.HasGesture", has_gesture);

  ukm::builders::MainFrameDownload(source_id)
      .SetHasGesture(has_gesture)
      .Record(ukm_recorder);
}

// static
void DownloadStats::RecordSubframeDownloadFlags(
    const DownloadFlags& flags,
    ukm::SourceId source_id,
    ukm::UkmRecorder* ukm_recorder) {
  UMA_HISTOGRAM_ENUMERATION("Download.Subframe.SandboxOriginAdGesture",
                            ToUmaValue(flags), kCountSandboxOriginAdGesture);

  ukm::builders::SubframeDownload(source_id)
      .SetHasSandbox(flags.has_sandbox)
      .SetIsCrossOrigin(flags.is_cross_origin)
      .SetIsAdFrame(flags.is_ad_frame)
      .SetHasGesture(flags.has_gesture)
      .Record(ukm_recorder);
}

}  // namespace blink
