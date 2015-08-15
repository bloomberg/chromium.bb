// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_stats.h"
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
}  // namespace

namespace remoting {

ChromotingStats::ChromotingStats()
    : video_bandwidth_(
          base::TimeDelta::FromSeconds(kStatsUpdateFrequencyInSeconds)),
      video_frame_rate_(
          base::TimeDelta::FromSeconds(kStatsUpdateFrequencyInSeconds)),
      video_packet_rate_(
          base::TimeDelta::FromSeconds(kStatsUpdateFrequencyInSeconds)),
      video_capture_ms_(kLatencySampleSize),
      video_encode_ms_(kLatencySampleSize),
      video_decode_ms_(kLatencySampleSize),
      video_paint_ms_(kLatencySampleSize),
      round_trip_ms_(kLatencySampleSize) {}

ChromotingStats::~ChromotingStats() {
}

void ChromotingStats::SetUpdateUmaCallbacks(
    UpdateUmaCustomHistogramCallback update_uma_custom_counts_callback,
    UpdateUmaCustomHistogramCallback update_uma_custom_times_callback,
    UpdateUmaEnumHistogramCallback update_uma_enum_histogram_callback) {
  uma_custom_counts_updater_ = update_uma_custom_counts_callback;
  uma_custom_times_updater_ = update_uma_custom_times_callback;
  uma_enum_histogram_updater_ = update_uma_enum_histogram_callback;
}

void ChromotingStats::RecordVideoPacketStats(const VideoPacket& packet) {
  // Record this received packet, even if it is empty.
  video_packet_rate_.Record(1);

  // Record the RTT, even for empty packets, otherwise input events that
  // do not cause an on-screen change can give very large, bogus RTTs.
  if (packet.has_latest_event_timestamp() &&
      packet.latest_event_timestamp() > latest_input_event_timestamp_) {
    latest_input_event_timestamp_ = packet.latest_event_timestamp();

    base::TimeDelta round_trip_latency =
        base::Time::Now() -
        base::Time::FromInternalValue(packet.latest_event_timestamp());

    round_trip_ms_.Record(round_trip_latency.InMilliseconds());

    if (!uma_custom_times_updater_.is_null())
      uma_custom_times_updater_.Run(
          kRoundTripLatencyHistogram, round_trip_latency.InMilliseconds(),
          kLatencyHistogramMinMs, kLatencyHistogramMaxMs,
          kLatencyHistogramBuckets);
  }

  // If the packet is empty, there are no other stats to update.
  if (!packet.data().size())
    return;

  video_frame_rate_.Record(1);
  video_bandwidth_.Record(packet.data().size());

  if (packet.has_capture_time_ms()) {
    video_capture_ms_.Record(packet.capture_time_ms());
    if (!uma_custom_times_updater_.is_null())
      uma_custom_times_updater_.Run(
          kVideoCaptureLatencyHistogram, packet.capture_time_ms(),
          kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
          kVideoActionsHistogramsBuckets);
  }

  if (packet.has_encode_time_ms()) {
    video_encode_ms_.Record(packet.encode_time_ms());
    if (!uma_custom_times_updater_.is_null())
      uma_custom_times_updater_.Run(
          kVideoEncodeLatencyHistogram, packet.encode_time_ms(),
          kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
          kVideoActionsHistogramsBuckets);
  }
}

void ChromotingStats::RecordDecodeTime(double value) {
  video_decode_ms_.Record(value);
  if (!uma_custom_times_updater_.is_null())
    uma_custom_times_updater_.Run(
        kVideoDecodeLatencyHistogram, value, kVideoActionsHistogramsMinMs,
        kVideoActionsHistogramsMaxMs, kVideoActionsHistogramsBuckets);
}

void ChromotingStats::RecordPaintTime(double value) {
  video_paint_ms_.Record(value);
  if (!uma_custom_times_updater_.is_null())
    uma_custom_times_updater_.Run(
        kVideoPaintLatencyHistogram, value, kVideoActionsHistogramsMinMs,
        kVideoActionsHistogramsMaxMs, kVideoActionsHistogramsBuckets);
}

void ChromotingStats::UploadRateStatsToUma() {
  if (!uma_enum_histogram_updater_.is_null()) {
    uma_enum_histogram_updater_.Run(kVideoFrameRateHistogram,
                                    video_frame_rate(), kMaxFramesPerSec);
    uma_enum_histogram_updater_.Run(kVideoPacketRateHistogram,
                                    video_packet_rate(), kMaxFramesPerSec);
    uma_custom_counts_updater_.Run(
        kVideoBandwidthHistogram, video_bandwidth(), kBandwidthHistogramMinBps,
        kBandwidthHistogramMaxBps, kBandwidthHistogramBuckets);
  }
}

}  // namespace remoting
