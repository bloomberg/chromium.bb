/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VIDEO_VIDEO_ANALYZER_H_
#define VIDEO_VIDEO_ANALYZER_H_

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/task_queue/task_queue_base.h"
#include "api/video/video_source_interface.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "rtc_base/event.h"
#include "rtc_base/numerics/running_statistics.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/time_utils.h"
#include "test/layer_filtering_transport.h"
#include "test/rtp_file_writer.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {

class VideoAnalyzer : public PacketReceiver,
                      public Transport,
                      public rtc::VideoSinkInterface<VideoFrame> {
 public:
  using Statistics = RunningStatistics<double>;

  VideoAnalyzer(test::LayerFilteringTransport* transport,
                const std::string& test_label,
                double avg_psnr_threshold,
                double avg_ssim_threshold,
                int duration_frames,
                TimeDelta test_duration,
                FILE* graph_data_output_file,
                const std::string& graph_title,
                uint32_t ssrc_to_analyze,
                uint32_t rtx_ssrc_to_analyze,
                size_t selected_stream,
                int selected_sl,
                int selected_tl,
                bool is_quick_test_enabled,
                Clock* clock,
                std::string rtp_dump_name,
                TaskQueueBase* task_queue);
  ~VideoAnalyzer();

  virtual void SetReceiver(PacketReceiver* receiver);
  void SetSource(rtc::VideoSourceInterface<VideoFrame>* video_source,
                 bool respect_sink_wants);
  void SetCall(Call* call);
  void SetSendStream(VideoSendStream* stream);
  void SetReceiveStream(VideoReceiveStream* stream);
  void SetAudioReceiveStream(AudioReceiveStream* recv_stream);

  rtc::VideoSinkInterface<VideoFrame>* InputInterface();
  rtc::VideoSourceInterface<VideoFrame>* OutputInterface();

  DeliveryStatus DeliverPacket(MediaType media_type,
                               rtc::CopyOnWriteBuffer packet,
                               int64_t packet_time_us) override;

  void PreEncodeOnFrame(const VideoFrame& video_frame);
  void PostEncodeOnFrame(size_t stream_id, uint32_t timestamp);

  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) override;

  bool SendRtcp(const uint8_t* packet, size_t length) override;
  void OnFrame(const VideoFrame& video_frame) override;
  void Wait();

  void StartMeasuringCpuProcessTime();
  void StopMeasuringCpuProcessTime();
  void StartExcludingCpuThreadTime();
  void StopExcludingCpuThreadTime();
  double GetCpuUsagePercent();

  test::LayerFilteringTransport* const transport_;
  PacketReceiver* receiver_;

 private:
  struct FrameComparison {
    FrameComparison();
    FrameComparison(const VideoFrame& reference,
                    const VideoFrame& render,
                    bool dropped,
                    int64_t input_time_ms,
                    int64_t send_time_ms,
                    int64_t recv_time_ms,
                    int64_t render_time_ms,
                    size_t encoded_frame_size);
    FrameComparison(bool dropped,
                    int64_t input_time_ms,
                    int64_t send_time_ms,
                    int64_t recv_time_ms,
                    int64_t render_time_ms,
                    size_t encoded_frame_size);

    absl::optional<VideoFrame> reference;
    absl::optional<VideoFrame> render;
    bool dropped;
    int64_t input_time_ms;
    int64_t send_time_ms;
    int64_t recv_time_ms;
    int64_t render_time_ms;
    size_t encoded_frame_size;
  };

  struct Sample {
    Sample(int dropped,
           int64_t input_time_ms,
           int64_t send_time_ms,
           int64_t recv_time_ms,
           int64_t render_time_ms,
           size_t encoded_frame_size,
           double psnr,
           double ssim);

    int dropped;
    int64_t input_time_ms;
    int64_t send_time_ms;
    int64_t recv_time_ms;
    int64_t render_time_ms;
    size_t encoded_frame_size;
    double psnr;
    double ssim;
  };

  // Implements VideoSinkInterface to receive captured frames from a
  // FrameGeneratorCapturer. Implements VideoSourceInterface to be able to act
  // as a source to VideoSendStream.
  // It forwards all input frames to the VideoAnalyzer for later comparison and
  // forwards the captured frames to the VideoSendStream.
  class CapturedFrameForwarder : public rtc::VideoSinkInterface<VideoFrame>,
                                 public rtc::VideoSourceInterface<VideoFrame> {
   public:
    CapturedFrameForwarder(VideoAnalyzer* analyzer,
                           Clock* clock,
                           int frames_to_capture,
                           TimeDelta test_duration);
    void SetSource(rtc::VideoSourceInterface<VideoFrame>* video_source);

   private:
    void OnFrame(const VideoFrame& video_frame) override;

    // Called when |send_stream_.SetSource()| is called.
    void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                         const rtc::VideoSinkWants& wants) override;

    // Called by |send_stream_| when |send_stream_.SetSource()| is called.
    void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override;

    VideoAnalyzer* const analyzer_;
    rtc::CriticalSection crit_;
    rtc::VideoSinkInterface<VideoFrame>* send_stream_input_
        RTC_GUARDED_BY(crit_);
    VideoSourceInterface<VideoFrame>* video_source_;
    Clock* clock_;
    int captured_frames_ RTC_GUARDED_BY(crit_);
    const int frames_to_capture_;
    const Timestamp test_end_;
  };

  struct FrameWithPsnr {
    double psnr;
    VideoFrame frame;
  };

  bool IsInSelectedSpatialAndTemporalLayer(const RtpPacket& rtp_packet);

  void AddFrameComparison(const VideoFrame& reference,
                          const VideoFrame& render,
                          bool dropped,
                          int64_t render_time_ms)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  void PollStats();
  static void FrameComparisonThread(void* obj);
  bool CompareFrames();
  bool PopComparison(FrameComparison* comparison);
  // Increment counter for number of frames received for comparison.
  void FrameRecorded();
  // Returns true if all frames to be compared have been taken from the queue.
  bool AllFramesRecorded();
  // Increase count of number of frames processed. Returns true if this was the
  // last frame to be processed.
  bool FrameProcessed();
  void PrintResults();
  void PerformFrameComparison(const FrameComparison& comparison);
  void PrintResult(const char* result_type,
                   Statistics stats,
                   const char* unit,
                   webrtc::test::ImproveDirection improve_direction);
  void PrintResultWithExternalMean(
      const char* result_type,
      double mean,
      Statistics stats,
      const char* unit,
      webrtc::test::ImproveDirection improve_direction);
  void PrintSamplesToFile(void);
  void AddCapturedFrameForComparison(const VideoFrame& video_frame);

  Call* call_;
  VideoSendStream* send_stream_;
  VideoReceiveStream* receive_stream_;
  AudioReceiveStream* audio_receive_stream_;
  CapturedFrameForwarder captured_frame_forwarder_;
  const std::string test_label_;
  FILE* const graph_data_output_file_;
  const std::string graph_title_;
  const uint32_t ssrc_to_analyze_;
  const uint32_t rtx_ssrc_to_analyze_;
  const size_t selected_stream_;
  const int selected_sl_;
  const int selected_tl_;

  rtc::CriticalSection comparison_lock_;
  std::vector<Sample> samples_ RTC_GUARDED_BY(comparison_lock_);
  Statistics sender_time_ RTC_GUARDED_BY(comparison_lock_);
  Statistics receiver_time_ RTC_GUARDED_BY(comparison_lock_);
  Statistics network_time_ RTC_GUARDED_BY(comparison_lock_);
  Statistics psnr_ RTC_GUARDED_BY(comparison_lock_);
  Statistics ssim_ RTC_GUARDED_BY(comparison_lock_);
  Statistics end_to_end_ RTC_GUARDED_BY(comparison_lock_);
  Statistics rendered_delta_ RTC_GUARDED_BY(comparison_lock_);
  Statistics encoded_frame_size_ RTC_GUARDED_BY(comparison_lock_);
  Statistics encode_frame_rate_ RTC_GUARDED_BY(comparison_lock_);
  Statistics encode_time_ms_ RTC_GUARDED_BY(comparison_lock_);
  Statistics encode_usage_percent_ RTC_GUARDED_BY(comparison_lock_);
  double mean_decode_time_ms_ RTC_GUARDED_BY(comparison_lock_);
  Statistics decode_time_ms_ RTC_GUARDED_BY(comparison_lock_);
  Statistics decode_time_max_ms_ RTC_GUARDED_BY(comparison_lock_);
  Statistics media_bitrate_bps_ RTC_GUARDED_BY(comparison_lock_);
  Statistics fec_bitrate_bps_ RTC_GUARDED_BY(comparison_lock_);
  Statistics send_bandwidth_bps_ RTC_GUARDED_BY(comparison_lock_);
  Statistics memory_usage_ RTC_GUARDED_BY(comparison_lock_);
  Statistics audio_expand_rate_ RTC_GUARDED_BY(comparison_lock_);
  Statistics audio_accelerate_rate_ RTC_GUARDED_BY(comparison_lock_);
  Statistics audio_jitter_buffer_ms_ RTC_GUARDED_BY(comparison_lock_);
  Statistics pixels_ RTC_GUARDED_BY(comparison_lock_);
  // Rendered frame with worst PSNR is saved for further analysis.
  absl::optional<FrameWithPsnr> worst_frame_ RTC_GUARDED_BY(comparison_lock_);
  // Freeze metrics.
  Statistics time_between_freezes_ RTC_GUARDED_BY(comparison_lock_);
  uint32_t freeze_count_ RTC_GUARDED_BY(comparison_lock_);
  uint32_t total_freezes_duration_ms_ RTC_GUARDED_BY(comparison_lock_);
  uint32_t total_frames_duration_ms_ RTC_GUARDED_BY(comparison_lock_);
  double sum_squared_frame_durations_ RTC_GUARDED_BY(comparison_lock_);

  double decode_frame_rate_ RTC_GUARDED_BY(comparison_lock_);
  double render_frame_rate_ RTC_GUARDED_BY(comparison_lock_);

  size_t last_fec_bytes_;

  rtc::CriticalSection crit_;
  const int frames_to_process_;
  const Timestamp test_end_;
  int frames_recorded_ RTC_GUARDED_BY(comparison_lock_);
  int frames_processed_ RTC_GUARDED_BY(comparison_lock_);
  int captured_frames_ RTC_GUARDED_BY(comparison_lock_);
  int dropped_frames_ RTC_GUARDED_BY(comparison_lock_);
  int dropped_frames_before_first_encode_ RTC_GUARDED_BY(crit_);
  int dropped_frames_before_rendering_ RTC_GUARDED_BY(crit_);
  int64_t last_render_time_ RTC_GUARDED_BY(comparison_lock_);
  int64_t last_render_delta_ms_ RTC_GUARDED_BY(comparison_lock_);
  int64_t last_unfreeze_time_ms_ RTC_GUARDED_BY(comparison_lock_);
  uint32_t rtp_timestamp_delta_ RTC_GUARDED_BY(crit_);

  rtc::CriticalSection cpu_measurement_lock_;
  int64_t cpu_time_ RTC_GUARDED_BY(cpu_measurement_lock_);
  int64_t wallclock_time_ RTC_GUARDED_BY(cpu_measurement_lock_);

  std::deque<VideoFrame> frames_ RTC_GUARDED_BY(crit_);
  absl::optional<VideoFrame> last_rendered_frame_ RTC_GUARDED_BY(crit_);
  rtc::TimestampWrapAroundHandler wrap_handler_ RTC_GUARDED_BY(crit_);
  std::map<int64_t, int64_t> send_times_ RTC_GUARDED_BY(crit_);
  std::map<int64_t, int64_t> recv_times_ RTC_GUARDED_BY(crit_);
  std::map<int64_t, size_t> encoded_frame_sizes_ RTC_GUARDED_BY(crit_);
  absl::optional<uint32_t> first_encoded_timestamp_ RTC_GUARDED_BY(crit_);
  absl::optional<uint32_t> first_sent_timestamp_ RTC_GUARDED_BY(crit_);
  const double avg_psnr_threshold_;
  const double avg_ssim_threshold_;
  bool is_quick_test_enabled_;

  std::vector<rtc::PlatformThread*> comparison_thread_pool_;
  rtc::Event comparison_available_event_;
  std::deque<FrameComparison> comparisons_ RTC_GUARDED_BY(comparison_lock_);
  bool quit_ RTC_GUARDED_BY(comparison_lock_);
  rtc::Event done_;

  std::unique_ptr<VideoRtpDepacketizer> vp8_depacketizer_;
  std::unique_ptr<VideoRtpDepacketizer> vp9_depacketizer_;
  std::unique_ptr<test::RtpFileWriter> rtp_file_writer_;
  Clock* const clock_;
  const int64_t start_ms_;
  TaskQueueBase* task_queue_;
};

}  // namespace webrtc
#endif  // VIDEO_VIDEO_ANALYZER_H_
