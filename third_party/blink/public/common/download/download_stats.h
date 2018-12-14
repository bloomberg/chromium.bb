// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_DOWNLOAD_DOWNLOAD_STATS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_DOWNLOAD_DOWNLOAD_STATS_H_

#include <stdint.h>

#include "base/macros.h"
#include "third_party/blink/public/common/common_export.h"

namespace ukm {

class UkmRecorder;

typedef int64_t SourceId;

}  // namespace ukm

namespace blink {

class BLINK_COMMON_EXPORT DownloadStats {
 public:
  struct DownloadFlags {
    bool has_sandbox;
    bool is_cross_origin;
    bool is_ad_frame;
    bool has_gesture;

    DownloadFlags() = default;

   private:
    DISALLOW_COPY_AND_ASSIGN(DownloadFlags);
  };

  // It's only public for testing purposes.
  static unsigned ToUmaValue(const DownloadFlags& flags);

  // Log the gesture bit for main frame download to UMA and UKM.
  static void RecordMainFrameHasGesture(bool has_gesture,
                                        ukm::SourceId source_id,
                                        ukm::UkmRecorder* ukm_recorder);

  // Log the bits in |flags| for a subframe download to UMA and UKM.
  static void RecordSubframeDownloadFlags(const DownloadFlags& flags,
                                          ukm::SourceId source_id,
                                          ukm::UkmRecorder* ukm_recorder);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(DownloadStats);

  // These values are used to construct an enum in UMA. They should never be
  // changed.
  static constexpr unsigned kGestureBit = 0x1 << 0;
  static constexpr unsigned kAdBit = 0x1 << 1;
  static constexpr unsigned kCrossOriginBit = 0x1 << 2;
  static constexpr unsigned kSandboxBit = 0x1 << 3;
  static constexpr unsigned kCountSandboxOriginAdGesture = 16;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_DOWNLOAD_DOWNLOAD_STATS_H_
