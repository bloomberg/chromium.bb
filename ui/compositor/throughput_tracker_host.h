// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_THROUGHPUT_TRACKER_HOST_H_
#define UI_COMPOSITOR_THROUGHPUT_TRACKER_HOST_H_

#include "base/callback_forward.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

// An interface for ThroughputTracker to call its host.
class COMPOSITOR_EXPORT ThroughputTrackerHost {
 public:
  using TrackerId = size_t;

  virtual ~ThroughputTrackerHost() = default;

  // Starts the tracking for the given tracker id. |callback| is invoked after
  // the tracker is stopped and the throughput data is collected.
  using ReportCallback = base::OnceCallback<void(
      const cc::FrameSequenceMetrics::ThroughputData throughput)>;
  virtual void StartThroughputTracker(TrackerId tracker_id,
                                      ReportCallback callback) = 0;

  // Stops the tracking for the given tracker id.
  virtual void StopThroughtputTracker(TrackerId tracker_id) = 0;

  // Cancels the tracking for the given tracker id.
  virtual void CancelThroughtputTracker(TrackerId tracker_id) = 0;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_THROUGHPUT_TRACKER_HOST_H_
