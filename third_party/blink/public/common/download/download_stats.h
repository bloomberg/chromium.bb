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
  struct BLINK_COMMON_EXPORT MainFrameDownloadFlags {
    bool has_sandbox;
    bool has_gesture;

    MainFrameDownloadFlags() = default;

    unsigned ToUmaValue() const;

   private:
    DISALLOW_COPY_AND_ASSIGN(MainFrameDownloadFlags);
  };

  struct BLINK_COMMON_EXPORT SubframeDownloadFlags {
    bool has_sandbox;
    bool is_cross_origin;
    bool is_ad_frame;
    bool has_gesture;

    SubframeDownloadFlags() = default;

    unsigned ToUmaValue() const;

   private:
    DISALLOW_COPY_AND_ASSIGN(SubframeDownloadFlags);
  };

  // Log the bits in |flags| for a main frame download to UMA and UKM.
  static void RecordMainFrameDownloadFlags(const MainFrameDownloadFlags& flags,
                                           ukm::SourceId source_id,
                                           ukm::UkmRecorder* ukm_recorder);

  // Log the bits in |flags| for a subframe download to UMA and UKM.
  static void RecordSubframeDownloadFlags(const SubframeDownloadFlags& flags,
                                          ukm::SourceId source_id,
                                          ukm::UkmRecorder* ukm_recorder);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(DownloadStats);

  // These values are used to construct an enum in UMA. They should never be
  // changed.
  static constexpr unsigned kSubframeGestureBit = 0x1 << 0;
  static constexpr unsigned kSubframeAdBit = 0x1 << 1;
  static constexpr unsigned kSubframeCrossOriginBit = 0x1 << 2;
  static constexpr unsigned kSubframeSandboxBit = 0x1 << 3;
  static constexpr unsigned kCountSandboxOriginAdGesture = 16;

  // These values are used to construct an enum in UMA. They should never be
  // changed.
  static constexpr unsigned kMainFrameGestureBit = 0x1 << 0;
  static constexpr unsigned kMainFrameSandboxBit = 0x1 << 1;
  static constexpr unsigned kCountSandboxGesture = 4;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_DOWNLOAD_DOWNLOAD_STATS_H_
