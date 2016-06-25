// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FRAME_STATS_H_
#define REMOTING_PROTOCOL_FRAME_STATS_H_

#include "base/time/time.h"

namespace remoting {

class VideoPacket;

namespace protocol {

// Struct used to track timestamp for various events in the video pipeline for a
// single video frame
struct FrameStats {
  FrameStats();
  FrameStats(const FrameStats&);
  ~FrameStats();

  // Copies timing fields from the |packet|.
  static FrameStats GetForVideoPacket(const VideoPacket& packet);

  int frame_size = 0;

  // Set to null for frames that were not sent after a fresh input event.
  base::TimeTicks latest_event_timestamp;

  // Set to TimeDelta::Max() when unknown.
  base::TimeDelta capture_delay = base::TimeDelta::Max();
  base::TimeDelta encode_delay = base::TimeDelta::Max();
  base::TimeDelta capture_pending_delay = base::TimeDelta::Max();
  base::TimeDelta capture_overhead_delay = base::TimeDelta::Max();
  base::TimeDelta encode_pending_delay = base::TimeDelta::Max();
  base::TimeDelta send_pending_delay = base::TimeDelta::Max();

  base::TimeTicks time_received;
  base::TimeTicks time_decoded;
  base::TimeTicks time_rendered;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FRAME_STATS_H_
