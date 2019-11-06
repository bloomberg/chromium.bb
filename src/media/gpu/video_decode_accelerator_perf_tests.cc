// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "media/base/test_data_util.h"
#include "media/gpu/test/video_player/frame_renderer_dummy.h"
#include "media/gpu/test/video_player/video.h"
#include "media/gpu/test/video_player/video_decoder_client.h"
#include "media/gpu/test/video_player/video_player.h"
#include "media/gpu/test/video_player/video_player_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {

// Video decoder perf tests usage message. Make sure to also update the
// documentation under docs/media/gpu/video_decoder_perf_test_usage.md when
// making changes here.
constexpr const char* usage_msg =
    "usage: video_decode_accelerator_perf_tests\n"
    "           [-v=<level>] [--vmodule=<config>] [--use_vd] [--gtest_help]\n"
    "           [--help] [<video path>] [<video metadata path>]\n";

// Video decoder perf tests help message.
constexpr const char* help_msg =
    "Run the video decode accelerator performance tests on the video\n"
    "specified by <video path>. If no <video path> is given the default\n"
    "\"test-25fps.h264\" video will be used.\n"
    "\nThe <video metadata path> should specify the location of a json file\n"
    "containing the video's metadata, such as frame checksums. By default\n"
    "<video path>.json will be used.\n"
    "\nThe following arguments are supported:\n"
    "   -v                  enable verbose mode, e.g. -v=2.\n"
    "  --vmodule            enable verbose mode for the specified module,\n"
    "                       e.g. --vmodule=*media/gpu*=2.\n"
    "  --use_vd             use the new VD-based video decoders, instead of\n"
    "                       the default VDA-based video decoders.\n"
    "  --gtest_help         display the gtest help and exit.\n"
    "  --help               display this help and exit.\n";

media::test::VideoPlayerTestEnvironment* g_env;

// Default output folder used to store performance metrics.
constexpr const base::FilePath::CharType* kDefaultOutputFolder =
    FILE_PATH_LITERAL("perf_metrics");

// TODO(dstaessens@) Expand with more meaningful metrics.
struct PerformanceMetrics {
  // Total measurement duration.
  base::TimeDelta total_duration_;
  // The number of frames decoded.
  size_t frames_decoded_ = 0;
  // The overall number of frames decoded per second.
  double frames_per_second_ = 0.0;
  // The number of frames dropped because of the decoder running behind, only
  // relevant for capped performance tests.
  size_t frames_dropped_ = 0;
  // The average time between subsequent frame deliveries.
  double avg_frame_delivery_time_ms_ = 0.0;
  // The median time between decode start and frame delivery.
  double median_frame_decode_time_ms_ = 0.0;
};

// The performance evaluator can be plugged into the video player to collect
// various performance metrics.
// TODO(dstaessens@) Check and post warning when CPU frequency scaling is
// enabled as this affects test results.
class PerformanceEvaluator : public VideoFrameProcessor {
 public:
  // Create a new performance evaluator. The caller should makes sure
  // |frame_renderer| outlives the performance evaluator.
  explicit PerformanceEvaluator(const FrameRendererDummy* const frame_renderer)
      : frame_renderer_(frame_renderer) {}

  // Interface VideoFrameProcessor
  void ProcessVideoFrame(scoped_refptr<const VideoFrame> video_frame,
                         size_t frame_index) override;
  bool WaitUntilDone() override { return true; }

  // Start/Stop collecting performance metrics.
  void StartMeasuring();
  void StopMeasuring();

  // Write the collected performance metrics to file.
  void WriteMetricsToFile() const;

 private:
  // Start/end time of the measurement period.
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;

  // Time at which the previous frame was delivered.
  base::TimeTicks prev_frame_delivery_time_;
  // List of times between subsequent frame deliveries.
  std::vector<double> frame_delivery_times_;
  // List of times between decode start and frame delivery.
  std::vector<double> frame_decode_times_;

  // Collection of various performance metrics.
  PerformanceMetrics perf_metrics_;

  // Frame renderer used to get the dropped frame rate, owned by the creator of
  // the performance evaluator.
  const FrameRendererDummy* const frame_renderer_;
};

void PerformanceEvaluator::ProcessVideoFrame(
    scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  base::TimeTicks now = base::TimeTicks::Now();

  base::TimeDelta delivery_time = (now - prev_frame_delivery_time_);
  frame_delivery_times_.push_back(delivery_time.InMillisecondsF());
  prev_frame_delivery_time_ = now;

  base::TimeDelta decode_time = now.since_origin() - video_frame->timestamp();
  frame_decode_times_.push_back(decode_time.InMillisecondsF());

  perf_metrics_.frames_decoded_++;
}

void PerformanceEvaluator::StartMeasuring() {
  start_time_ = base::TimeTicks::Now();
  prev_frame_delivery_time_ = start_time_;
}

void PerformanceEvaluator::StopMeasuring() {
  end_time_ = base::TimeTicks::Now();
  perf_metrics_.total_duration_ = end_time_ - start_time_;
  perf_metrics_.frames_per_second_ = perf_metrics_.frames_decoded_ /
                                     perf_metrics_.total_duration_.InSecondsF();
  perf_metrics_.frames_dropped_ = frame_renderer_->FramesDropped();

  perf_metrics_.avg_frame_delivery_time_ms_ =
      perf_metrics_.total_duration_.InMillisecondsF() /
      perf_metrics_.frames_decoded_;

  std::sort(frame_decode_times_.begin(), frame_decode_times_.end());
  size_t median_index = frame_decode_times_.size() / 2;
  perf_metrics_.median_frame_decode_time_ms_ =
      (frame_decode_times_.size() % 2 != 0)
          ? frame_decode_times_[median_index]
          : (frame_decode_times_[median_index - 1] +
             frame_decode_times_[median_index]) /
                2.0;

  VLOG(0) << "Frames decoded: " << perf_metrics_.frames_decoded_;
  VLOG(0) << "Total duration: "
          << perf_metrics_.total_duration_.InMillisecondsF() << "ms";
  VLOG(0) << "FPS:            " << perf_metrics_.frames_per_second_;
  VLOG(0) << "Frames Dropped: " << perf_metrics_.frames_dropped_;
  VLOG(0) << "Avg. frame delivery time:   "
          << perf_metrics_.avg_frame_delivery_time_ms_ << "ms";
  VLOG(0) << "Median frame decode time:   "
          << perf_metrics_.median_frame_decode_time_ms_ << "ms";
}

void PerformanceEvaluator::WriteMetricsToFile() const {
  std::string str = base::StringPrintf(
      "Frames decoded: %zu\nTotal duration: %fms\nFPS: %f\n"
      "Frames dropped: %zu\nAvg. frame delivery time: %fms\n"
      "Median frame decode time: %fms\n",
      perf_metrics_.frames_decoded_,
      perf_metrics_.total_duration_.InMillisecondsF(),
      perf_metrics_.frames_per_second_, perf_metrics_.frames_dropped_,
      perf_metrics_.avg_frame_delivery_time_ms_,
      perf_metrics_.median_frame_decode_time_ms_);

  // Write performance metrics to file.
  base::FilePath output_folder_path = base::FilePath(kDefaultOutputFolder);
  if (!DirectoryExists(output_folder_path))
    base::CreateDirectory(output_folder_path);
  base::FilePath output_file_path =
      output_folder_path.Append(base::FilePath(g_env->GetTestName())
                                    .AddExtension(FILE_PATH_LITERAL(".txt")));
  base::File output_file(
      base::FilePath(output_file_path),
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  output_file.WriteAtCurrentPos(str.data(), str.length());
  VLOG(0) << "Wrote performance metrics to: " << output_file_path;

  // Write frame decode times to file.
  base::FilePath decode_times_file_path =
      output_file_path.InsertBeforeExtension(FILE_PATH_LITERAL(".frame_times"));
  base::File decode_times_output_file(
      base::FilePath(decode_times_file_path),
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  for (double frame_decoded_time : frame_delivery_times_) {
    std::string decode_time_str =
        base::StringPrintf("%f\n", frame_decoded_time);
    decode_times_output_file.WriteAtCurrentPos(decode_time_str.data(),
                                               decode_time_str.length());
  }
  VLOG(0) << "Wrote frame decode times to: " << decode_times_file_path;
}

// Video decode test class. Performs setup and teardown for each single test.
class VideoDecoderTest : public ::testing::Test {
 public:
  // Create a new video player instance. |render_frame_rate| is the rate at
  // which the video player will simulate rendering frames, if 0 no rendering is
  // simulated. The |vsync_rate| is used during simulated rendering, if 0 Vsync
  // is disabled.
  std::unique_ptr<VideoPlayer> CreateVideoPlayer(const Video* video,
                                                 uint32_t render_frame_rate = 0,
                                                 uint32_t vsync_rate = 0) {
    LOG_ASSERT(video);

    // Create dummy frame renderer, simulates rendering at specified frame rate.
    base::TimeDelta frame_duration;
    base::TimeDelta vsync_interval_duration;
    if (render_frame_rate > 0) {
      frame_duration = base::TimeDelta::FromSeconds(1) / render_frame_rate;
      vsync_interval_duration = base::TimeDelta::FromSeconds(1) / vsync_rate;
    }
    auto frame_renderer =
        FrameRendererDummy::Create(frame_duration, vsync_interval_duration);

    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors;
    auto performance_evaluator =
        std::make_unique<PerformanceEvaluator>(frame_renderer.get());
    performance_evaluator_ = performance_evaluator.get();
    frame_processors.push_back(std::move(performance_evaluator));

    // Use the new VD-based video decoders if requested.
    VideoDecoderClientConfig config;
    config.use_vd = g_env->UseVD();

    return VideoPlayer::Create(video, std::move(frame_renderer),
                               std::move(frame_processors), config);
  }

  PerformanceEvaluator* performance_evaluator_;
};

}  // namespace

// Play video from start to end while measuring uncapped performance. This test
// will decode a video as fast as possible, and gives an idea about the maximum
// output of the decoder.
TEST_F(VideoDecoderTest, MeasureUncappedPerformance) {
  auto tvp = CreateVideoPlayer(g_env->Video());

  performance_evaluator_->StartMeasuring();
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  performance_evaluator_->StopMeasuring();
  performance_evaluator_->WriteMetricsToFile();

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
}

// Play video from start to end while measuring capped performance. This test
// will simulate rendering the video at its actual frame rate, and will
// calculate the number of frames that were dropped. Vsync is enabled at 60 FPS.
TEST_F(VideoDecoderTest, MeasureCappedPerformance) {
  auto tvp = CreateVideoPlayer(g_env->Video(), g_env->Video()->FrameRate(), 60);

  performance_evaluator_->StartMeasuring();
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  tvp->WaitForRenderer();
  performance_evaluator_->StopMeasuring();
  performance_evaluator_->WriteMetricsToFile();

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
}

}  // namespace test
}  // namespace media

int main(int argc, char** argv) {
  // Set the default test data path.
  media::test::Video::SetTestDataPath(media::GetTestDataPath());

  // Print the help message if requested. This needs to be done before
  // initializing gtest, to overwrite the default gtest help message.
  base::CommandLine::Init(argc, argv);
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  LOG_ASSERT(cmd_line);
  if (cmd_line->HasSwitch("help")) {
    std::cout << media::test::usage_msg << "\n" << media::test::help_msg;
    return 0;
  }

  // Check if a video was specified on the command line.
  base::CommandLine::StringVector args = cmd_line->GetArgs();
  base::FilePath video_path =
      (args.size() >= 1) ? base::FilePath(args[0]) : base::FilePath();
  base::FilePath video_metadata_path =
      (args.size() >= 2) ? base::FilePath(args[1]) : base::FilePath();

  // Parse command line arguments.
  bool use_vd = false;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||               // Handled by GoogleTest
        it->first == "v" || it->first == "vmodule") {  // Handled by Chrome
      continue;
    }

    if (it->first == "use_vd") {
      use_vd = true;
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::test::usage_msg;
      return EXIT_FAILURE;
    }
  }

  testing::InitGoogleTest(&argc, argv);

  // Set up our test environment.
  media::test::VideoPlayerTestEnvironment* test_environment =
      media::test::VideoPlayerTestEnvironment::Create(
          video_path, video_metadata_path, false, false, use_vd);
  if (!test_environment)
    return EXIT_FAILURE;

  media::test::g_env = static_cast<media::test::VideoPlayerTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));

  return RUN_ALL_TESTS();
}
