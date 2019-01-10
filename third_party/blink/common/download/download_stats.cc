// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/download/download_stats.h"

#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace blink {

unsigned DownloadStats::MainFrameDownloadFlags::ToUmaValue() const {
  unsigned value = 0;
  if (has_sandbox)
    value |= kMainFrameSandboxBit;
  if (has_gesture)
    value |= kMainFrameGestureBit;
  return value;
}

unsigned DownloadStats::SubframeDownloadFlags::ToUmaValue() const {
  unsigned value = 0;
  if (has_sandbox)
    value |= kSubframeSandboxBit;
  if (is_cross_origin)
    value |= kSubframeCrossOriginBit;
  if (is_ad_frame)
    value |= kSubframeAdBit;
  if (has_gesture)
    value |= kSubframeGestureBit;
  return value;
}

void DownloadStats::RecordMainFrameDownloadFlags(
    const MainFrameDownloadFlags& flags,
    ukm::SourceId source_id,
    ukm::UkmRecorder* ukm_recorder) {
  UMA_HISTOGRAM_ENUMERATION("Download.MainFrame.SandboxGesture",
                            flags.ToUmaValue(), kCountSandboxGesture);

  ukm::builders::MainFrameDownload(source_id)
      .SetHasSandbox(flags.has_sandbox)
      .SetHasGesture(flags.has_gesture)
      .Record(ukm_recorder);
}

// static
void DownloadStats::RecordSubframeDownloadFlags(
    const SubframeDownloadFlags& flags,
    ukm::SourceId source_id,
    ukm::UkmRecorder* ukm_recorder) {
  UMA_HISTOGRAM_ENUMERATION("Download.Subframe.SandboxOriginAdGesture",
                            flags.ToUmaValue(), kCountSandboxOriginAdGesture);

  ukm::builders::SubframeDownload(source_id)
      .SetHasSandbox(flags.has_sandbox)
      .SetIsCrossOrigin(flags.is_cross_origin)
      .SetIsAdFrame(flags.is_ad_frame)
      .SetHasGesture(flags.has_gesture)
      .Record(ukm_recorder);
}

}  // namespace blink
