/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_VIDEO_QUALITY_ANALYZER_INJECTION_HELPER_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_VIDEO_QUALITY_ANALYZER_INJECTION_HELPER_H_

#include <map>
#include <memory>
#include <string>

#include "api/test/peerconnection_quality_test_fixture.h"
#include "api/test/stats_observer_interface.h"
#include "api/test/video_quality_analyzer_interface.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "rtc_base/critical_section.h"
#include "test/pc/e2e/analyzer/video/encoded_image_data_injector.h"
#include "test/pc/e2e/analyzer/video/id_generator.h"
#include "test/test_video_capturer.h"
#include "test/testsupport/video_frame_writer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Provides factory methods for components, that will be used to inject
// VideoQualityAnalyzerInterface into PeerConnection pipeline.
class VideoQualityAnalyzerInjectionHelper : public StatsObserverInterface {
 public:
  using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;

  VideoQualityAnalyzerInjectionHelper(
      std::unique_ptr<VideoQualityAnalyzerInterface> analyzer,
      EncodedImageDataInjector* injector,
      EncodedImageDataExtractor* extractor);
  ~VideoQualityAnalyzerInjectionHelper() override;

  // Wraps video encoder factory to give video quality analyzer access to frames
  // before encoding and encoded images after.
  std::unique_ptr<VideoEncoderFactory> WrapVideoEncoderFactory(
      std::unique_ptr<VideoEncoderFactory> delegate,
      double bitrate_multiplier,
      std::map<std::string, absl::optional<int>> stream_required_spatial_index)
      const;
  // Wraps video decoder factory to give video quality analyzer access to
  // received encoded images and frames, that were decoded from them.
  std::unique_ptr<VideoDecoderFactory> WrapVideoDecoderFactory(
      std::unique_ptr<VideoDecoderFactory> delegate) const;

  // Creates VideoFrame preprocessor, that will allow video quality analyzer to
  // get access to the captured frames. If provided config also specifies
  // |input_dump_file_name|, video will be written into that file.
  std::unique_ptr<test::TestVideoCapturer::FramePreprocessor>
  CreateFramePreprocessor(const VideoConfig& config);
  // Creates sink, that will allow video quality analyzer to get access to
  // the rendered frames. If corresponding video track has
  // |output_dump_file_name| in its VideoConfig, then video also will be written
  // into that file.
  std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>> CreateVideoSink();

  void Start(std::string test_case_name, int max_threads_count);

  // Forwards |stats_reports| for Peer Connection |pc_label| to
  // |analyzer_|.
  void OnStatsReports(const std::string& pc_label,
                      const StatsReports& stats_reports) override;

  // Stops VideoQualityAnalyzerInterface to populate final data and metrics.
  // Should be invoked after analyzed video tracks are disposed.
  void Stop();

 private:
  class AnalyzingVideoSink final : public rtc::VideoSinkInterface<VideoFrame> {
   public:
    explicit AnalyzingVideoSink(VideoQualityAnalyzerInjectionHelper* helper)
        : helper_(helper) {}
    ~AnalyzingVideoSink() override = default;

    void OnFrame(const VideoFrame& frame) override { helper_->OnFrame(frame); }

   private:
    VideoQualityAnalyzerInjectionHelper* const helper_;
  };

  test::VideoFrameWriter* MaybeCreateVideoWriter(
      absl::optional<std::string> file_name,
      const PeerConnectionE2EQualityTestFixture::VideoConfig& config);
  void OnFrame(const VideoFrame& frame);
  std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>>*
  PopulateSinks(const std::string& stream_label);

  std::unique_ptr<VideoQualityAnalyzerInterface> analyzer_;
  EncodedImageDataInjector* injector_;
  EncodedImageDataExtractor* extractor_;

  std::vector<std::unique_ptr<test::VideoFrameWriter>> video_writers_;

  rtc::CriticalSection lock_;
  std::map<std::string, VideoConfig> known_video_configs_ RTC_GUARDED_BY(lock_);
  std::map<std::string,
           std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>>>
      sinks_ RTC_GUARDED_BY(lock_);

  std::unique_ptr<IdGenerator<int>> encoding_entities_id_generator_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_VIDEO_QUALITY_ANALYZER_INJECTION_HELPER_H_
