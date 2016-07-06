// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/frame_stats.h"

#include "remoting/proto/video.pb.h"

namespace remoting {
namespace protocol {

ClientFrameStats::ClientFrameStats() = default;
ClientFrameStats::ClientFrameStats(const ClientFrameStats&) = default;
ClientFrameStats::~ClientFrameStats() = default;
ClientFrameStats& ClientFrameStats::operator=(const ClientFrameStats&) =
    default;

HostFrameStats::HostFrameStats() = default;
HostFrameStats::HostFrameStats(const HostFrameStats&) = default;
HostFrameStats::~HostFrameStats() = default;

// static
HostFrameStats HostFrameStats::GetForVideoPacket(const VideoPacket& packet) {
  HostFrameStats result;
  result.frame_size = packet.data().size();
  if (packet.has_latest_event_timestamp()) {
    result.latest_event_timestamp =
        base::TimeTicks::FromInternalValue(packet.latest_event_timestamp());
  }
  if (packet.has_capture_time_ms()) {
    result.capture_delay =
        base::TimeDelta::FromMilliseconds(packet.capture_time_ms());
  }
  if (packet.has_encode_time_ms()) {
    result.encode_delay =
        base::TimeDelta::FromMilliseconds(packet.encode_time_ms());
  }
  if (packet.has_capture_pending_time_ms()) {
    result.capture_pending_delay =
        base::TimeDelta::FromMilliseconds(packet.capture_pending_time_ms());
  }
  if (packet.has_capture_overhead_time_ms()) {
    result.capture_overhead_delay =
        base::TimeDelta::FromMilliseconds(packet.capture_overhead_time_ms());
  }
  if (packet.has_encode_pending_time_ms()) {
    result.encode_pending_delay =
        base::TimeDelta::FromMilliseconds(packet.encode_pending_time_ms());
  }
  if (packet.has_send_pending_time_ms()) {
    result.send_pending_delay =
        base::TimeDelta::FromMilliseconds(packet.send_pending_time_ms());
  }
  return result;
}

FrameStats::FrameStats() = default;
FrameStats::FrameStats(const FrameStats&) = default;
FrameStats::~FrameStats() = default;

}  // namespace protocol
}  // namespace remoting
