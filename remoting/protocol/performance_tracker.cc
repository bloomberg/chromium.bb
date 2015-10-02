// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/performance_tracker.h"

#include "remoting/proto/video.pb.h"

namespace {

// We take the last 10 latency numbers and report the average.
const int kLatencySampleSize = 10;

// UMA histogram names.
const char kRoundTripLatencyHistogram[] = "Chromoting.Video.RoundTripLatency";
const char kVideoCaptureLatencyHistogram[] = "Chromoting.Video.CaptureLatency";
const char kVideoEncodeLatencyHistogram[] = "Chromoting.Video.EncodeLatency";
const char kVideoDecodeLatencyHistogram[] = "Chromoting.Video.DecodeLatency";
const char kVideoPaintLatencyHistogram[] = "Chromoting.Video.PaintLatency";
const char kVideoFrameRateHistogram[] = "Chromoting.Video.FrameRate";
const char kVideoPacketRateHistogram[] = "Chromoting.Video.PacketRate";
const char kVideoBandwidthHistogram[] = "Chromoting.Video.Bandwidth";
const char kCapturePendingLatencyHistogram[] =
    "Chromoting.Video.CapturePendingLatency";
const char kCaptureOverheadHistogram[] = "Chromoting.Video.CaptureOverhead";
const char kEncodePendingLatencyHistogram[] =
    "Chromoting.Video.EncodePendingLatency";
const char kSendPendingLatencyHistogram[] =
    "Chromoting.Video.SendPendingLatency";
const char kNetworkLatencyHistogram[] = "Chromoting.Video.NetworkLatency";

// Custom count and custom time histograms are log-scaled by default. This
// results in fine-grained buckets at lower values and wider-ranged buckets
// closer to the maximum.
// The values defined for each histogram below are based on the 99th percentile
// numbers for the corresponding metric over a recent 28-day period.
// Values above the maximum defined for a histogram end up in the max-bucket.
// If the minimum for a UMA histogram is set to be < 1, it is implicitly
// normalized to 1.
// See $/src/base/metrics/histogram.h for more details.

// Video-specific metrics are stored in a custom times histogram.
const int kVideoActionsHistogramsMinMs = 1;
const int kVideoActionsHistogramsMaxMs = 250;
const int kVideoActionsHistogramsBuckets = 50;

// Round-trip latency values are stored in a custom times histogram.
const int kLatencyHistogramMinMs = 1;
const int kLatencyHistogramMaxMs = 20000;
const int kLatencyHistogramBuckets = 50;

// Bandwidth statistics are stored in a custom counts histogram.
const int kBandwidthHistogramMinBps = 0;
const int kBandwidthHistogramMaxBps = 10 * 1000 * 1000;
const int kBandwidthHistogramBuckets = 100;

// Frame rate is stored in a custom enum histogram, because we we want to record
// the frequency of each discrete value, rather than using log-scaled buckets.
// We don't expect video frame rate to be greater than 40fps. Setting a maximum
// of 100fps will leave some room for future improvements, and account for any
// bursts of packets. Enum histograms expect samples to be less than the
// boundary value, so set to 101.
const int kMaxFramesPerSec = 101;


void UpdateUmaEnumHistogramStub(const std::string& histogram_name,
                                int64_t value,
                                int histogram_max) {}

void UpdateUmaCustomHistogramStub(const std::string& histogram_name,
                                  int64_t value,
                                  int histogram_min,
                                  int histogram_max,
                                  int histogram_buckets) {}
}  // namespace

namespace remoting {
namespace protocol {

PerformanceTracker::FrameTimestamps::FrameTimestamps() {}
PerformanceTracker::FrameTimestamps::~FrameTimestamps() {}

PerformanceTracker::PerformanceTracker()
    : video_bandwidth_(base::TimeDelta::FromSeconds(kStatsUpdatePeriodSeconds)),
      video_frame_rate_(
          base::TimeDelta::FromSeconds(kStatsUpdatePeriodSeconds)),
      video_packet_rate_(
          base::TimeDelta::FromSeconds(kStatsUpdatePeriodSeconds)),
      video_capture_ms_(kLatencySampleSize),
      video_encode_ms_(kLatencySampleSize),
      video_decode_ms_(kLatencySampleSize),
      video_paint_ms_(kLatencySampleSize),
      round_trip_ms_(kLatencySampleSize) {
  uma_custom_counts_updater_ = base::Bind(&UpdateUmaCustomHistogramStub);
  uma_custom_times_updater_ = base::Bind(&UpdateUmaCustomHistogramStub);
  uma_enum_histogram_updater_ = base::Bind(&UpdateUmaEnumHistogramStub);
}

PerformanceTracker::~PerformanceTracker() {}

void PerformanceTracker::SetUpdateUmaCallbacks(
    UpdateUmaCustomHistogramCallback update_uma_custom_counts_callback,
    UpdateUmaCustomHistogramCallback update_uma_custom_times_callback,
    UpdateUmaEnumHistogramCallback update_uma_enum_histogram_callback) {
  DCHECK(!update_uma_custom_counts_callback.is_null());
  DCHECK(!update_uma_custom_times_callback.is_null());
  DCHECK(!update_uma_enum_histogram_callback.is_null());

  uma_custom_counts_updater_ = update_uma_custom_counts_callback;
  uma_custom_times_updater_ = update_uma_custom_times_callback;
  uma_enum_histogram_updater_ = update_uma_enum_histogram_callback;
}

void PerformanceTracker::RecordVideoPacketStats(const VideoPacket& packet) {
  if (!is_paused_ && !upload_uma_stats_timer_.IsRunning()) {
    upload_uma_stats_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kStatsUpdatePeriodSeconds),
        base::Bind(&PerformanceTracker::UploadRateStatsToUma,
                   base::Unretained(this)));
  }

  // Record this received packet, even if it is empty.
  video_packet_rate_.Record(1);

  FrameTimestamps timestamps;
  timestamps.time_received = base::TimeTicks::Now();
  if (packet.has_latest_event_timestamp()) {
    base::TimeTicks timestamp =
        base::TimeTicks::FromInternalValue(packet.latest_event_timestamp());
    // Only use latest_event_timestamp field if it has changed from the
    // previous frame.
    if (timestamp > latest_event_timestamp_) {
      timestamps.latest_event_timestamp = timestamp;
      latest_event_timestamp_ = timestamp;
    }
  }

  // If the host didn't specify any of the latency fields then set
  // |total_host_latency| to Max, to indicate that the latency is unknown.
  timestamps.total_host_latency = base::TimeDelta::Max();
  if (packet.has_capture_time_ms() && packet.has_encode_time_ms() &&
      packet.has_capture_pending_time_ms() &&
      packet.has_capture_overhead_time_ms() &&
      packet.has_encode_pending_time_ms() &&
      packet.has_send_pending_time_ms()) {
    timestamps.total_host_latency = base::TimeDelta::FromMilliseconds(
        packet.capture_time_ms() + packet.encode_time_ms() +
        packet.capture_pending_time_ms() + packet.capture_overhead_time_ms() +
        packet.encode_pending_time_ms() + packet.send_pending_time_ms());
  }

  // If the packet is empty, there are no other stats to update.
  if (!packet.data().size()) {
    // Record the RTT, even for empty packets, otherwise input events that
    // do not cause an on-screen change can give very large, bogus RTTs.
    RecordRoundTripLatency(timestamps);
    return;
  }

  DCHECK(packet.has_frame_id());
  frame_timestamps_[packet.frame_id()] = timestamps;

  video_frame_rate_.Record(1);
  video_bandwidth_.Record(packet.data().size());

  if (packet.has_capture_time_ms()) {
    video_capture_ms_.Record(packet.capture_time_ms());
    uma_custom_times_updater_.Run(
        kVideoCaptureLatencyHistogram, packet.capture_time_ms(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }

  if (packet.has_encode_time_ms()) {
    video_encode_ms_.Record(packet.encode_time_ms());
    uma_custom_times_updater_.Run(
        kVideoEncodeLatencyHistogram, packet.encode_time_ms(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }

  if (packet.has_capture_pending_time_ms()) {
    uma_custom_times_updater_.Run(
        kCapturePendingLatencyHistogram, packet.capture_pending_time_ms(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }

  if (packet.has_capture_overhead_time_ms()) {
    uma_custom_times_updater_.Run(
        kCaptureOverheadHistogram, packet.capture_overhead_time_ms(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }

  if (packet.has_encode_pending_time_ms()) {
    uma_custom_times_updater_.Run(
        kEncodePendingLatencyHistogram, packet.encode_pending_time_ms(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }

  if (packet.has_send_pending_time_ms()) {
    uma_custom_times_updater_.Run(
        kSendPendingLatencyHistogram, packet.send_pending_time_ms(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }
}

void PerformanceTracker::OnFrameDecoded(int32_t frame_id) {
  FramesTimestampsMap::iterator it = frame_timestamps_.find(frame_id);
  DCHECK(it != frame_timestamps_.end());
  it->second.time_decoded = base::TimeTicks::Now();
  base::TimeDelta delay = it->second.time_decoded - it->second.time_received;

  video_decode_ms_.Record(delay.InMilliseconds());
  uma_custom_times_updater_.Run(
      kVideoDecodeLatencyHistogram, delay.InMilliseconds(),
      kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
      kVideoActionsHistogramsBuckets);
}

void PerformanceTracker::OnFramePainted(int32_t frame_id) {
  base::TimeTicks now = base::TimeTicks::Now();

  while (!frame_timestamps_.empty() &&
         frame_timestamps_.begin()->first <= frame_id) {
    FrameTimestamps& timestamps = frame_timestamps_.begin()->second;

    // time_decoded may be null if OnFrameDecoded() was never called, e.g. if
    // the frame was dropped or decoding has failed.
    if (!timestamps.time_decoded.is_null()) {
      base::TimeDelta delay = now - timestamps.time_decoded;
      video_paint_ms_.Record(delay.InMilliseconds());
      uma_custom_times_updater_.Run(
          kVideoPaintLatencyHistogram, delay.InMilliseconds(),
          kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
          kVideoActionsHistogramsBuckets);
    }

    RecordRoundTripLatency(timestamps);
    frame_timestamps_.erase(frame_timestamps_.begin());
  }
}

void PerformanceTracker::RecordRoundTripLatency(
    const FrameTimestamps& timestamps) {
  if (timestamps.latest_event_timestamp.is_null())
    return;

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta round_trip_latency =
      now - timestamps.latest_event_timestamp;

  round_trip_ms_.Record(round_trip_latency.InMilliseconds());
  uma_custom_times_updater_.Run(
      kRoundTripLatencyHistogram, round_trip_latency.InMilliseconds(),
      kLatencyHistogramMinMs, kLatencyHistogramMaxMs, kLatencyHistogramBuckets);

  if (!timestamps.total_host_latency.is_max()) {
    // Calculate total processing time on host and client.
    base::TimeDelta total_processing_latency =
        timestamps.total_host_latency + (now - timestamps.time_received);
    base::TimeDelta network_latency =
        round_trip_latency - total_processing_latency;
    uma_custom_times_updater_.Run(
        kNetworkLatencyHistogram, network_latency.InMilliseconds(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }
}

void PerformanceTracker::UploadRateStatsToUma() {
  uma_enum_histogram_updater_.Run(kVideoFrameRateHistogram, video_frame_rate(),
                                  kMaxFramesPerSec);
  uma_enum_histogram_updater_.Run(kVideoPacketRateHistogram,
                                  video_packet_rate(), kMaxFramesPerSec);
  uma_custom_counts_updater_.Run(
      kVideoBandwidthHistogram, video_bandwidth(), kBandwidthHistogramMinBps,
      kBandwidthHistogramMaxBps, kBandwidthHistogramBuckets);
}

void PerformanceTracker::OnPauseStateChanged(bool paused) {
  is_paused_ = paused;
  if (is_paused_) {
    // Pause the UMA timer when the video is paused. It will be unpaused in
    // RecordVideoPacketStats() when a new frame is received.
    upload_uma_stats_timer_.Stop();
  }
}

}  // namespace protocol
}  // namespace remoting
