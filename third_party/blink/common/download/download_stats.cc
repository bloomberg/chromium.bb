// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/download/download_stats.h"

#include "base/metrics/histogram_macros.h"

namespace blink {

// static
void DownloadStats::RecordMainFrameHasGesture(bool gesture) {
  UMA_HISTOGRAM_BOOLEAN("Download.MainFrame.HasGesture", gesture);
}

// static
void DownloadStats::RecordSubframeSandboxOriginAdGesture(unsigned value) {
  UMA_HISTOGRAM_ENUMERATION("Download.Subframe.SandboxOriginAdGesture", value,
                            kCountSandboxOriginAdGesture);
}

}  // namespace blink
