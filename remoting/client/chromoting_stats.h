// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ChromotingStats defines a bundle of performance counters and statistics
// for chromoting.

#ifndef REMOTING_CLIENT_CHROMOTING_STATS_H_
#define REMOTING_CLIENT_CHROMOTING_STATS_H_

#include "base/callback.h"
#include "remoting/base/rate_counter.h"
#include "remoting/base/running_average.h"

namespace remoting {

class VideoPacket;

class ChromotingStats {
 public:
  // Callback that updates UMA custom counts or custom times histograms.
  typedef base::Callback<void(const std::string& histogram_name,
                              int64 value,
                              int histogram_min,
                              int histogram_max,
                              int histogram_buckets)>
      UpdateUmaCustomHistogramCallback;

  // Callback that updates UMA enumeration histograms.
  typedef base::Callback<
      void(const std::string& histogram_name, int64 value, int histogram_max)>
      UpdateUmaEnumHistogramCallback;

  ChromotingStats();
  virtual ~ChromotingStats();

  // Constant used to calculate the average for rate metrics and used by the
  // plugin for the frequency at which stats should be updated.
  static const int kStatsUpdateFrequencyInSeconds = 1;

  // Return rates and running-averages for different metrics.
  double video_bandwidth() { return video_bandwidth_.Rate(); }
  double video_frame_rate() { return video_frame_rate_.Rate(); }
  double video_packet_rate() { return video_packet_rate_.Rate(); }
  double video_capture_ms() { return video_capture_ms_.Average(); }
  double video_encode_ms() { return video_encode_ms_.Average(); }
  double video_decode_ms() { return video_decode_ms_.Average(); }
  double video_paint_ms() { return video_paint_ms_.Average(); }
  double round_trip_ms() { return round_trip_ms_.Average(); }

  // Record stats for a video-packet.
  void RecordVideoPacketStats(const VideoPacket& packet);

  void RecordDecodeTime(double value);
  void RecordPaintTime(double value);

  // Sets callbacks in ChromotingInstance to update a UMA custom counts, custom
  // times or enum histogram.
  void SetUpdateUmaCallbacks(
      UpdateUmaCustomHistogramCallback update_uma_custom_counts_callback,
      UpdateUmaCustomHistogramCallback update_uma_custom_times_callback,
      UpdateUmaEnumHistogramCallback update_uma_enum_histogram_callback);

  // Updates frame-rate, packet-rate and bandwidth UMA statistics.
  void UploadRateStatsToUma();

 private:
  // The video and packet rate metrics below are updated per video packet
  // received and then, for reporting, averaged over a 1s time-window.
  // Bytes per second for non-empty video-packets.
  RateCounter video_bandwidth_;

  // Frames per second for non-empty video-packets.
  RateCounter video_frame_rate_;

  // Video packets per second, including empty video-packets.
  // This will be greater than the frame rate, as individual frames are
  // contained in packets, some of which might be empty (e.g. when there are no
  // screen changes).
  RateCounter video_packet_rate_;

  // The following running-averages are uploaded to UMA per video packet and
  // also used for display to users, averaged over the N most recent samples.
  // N = kLatencySampleSize.
  RunningAverage video_capture_ms_;
  RunningAverage video_encode_ms_;
  RunningAverage video_decode_ms_;
  RunningAverage video_paint_ms_;
  RunningAverage round_trip_ms_;

  // Used to update UMA stats, if set.
  UpdateUmaCustomHistogramCallback uma_custom_counts_updater_;
  UpdateUmaCustomHistogramCallback uma_custom_times_updater_;
  UpdateUmaEnumHistogramCallback uma_enum_histogram_updater_;

  // The latest input timestamp that a VideoPacket was seen annotated with.
  int64 latest_input_event_timestamp_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ChromotingStats);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_STATS_H_
