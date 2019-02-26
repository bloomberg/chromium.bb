// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/test/scoped_task_environment.h"

#include "media/base/test_data_util.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/test/video_player/frame_renderer_dummy.h"
#include "media/gpu/test/video_player/video.h"
#include "media/gpu/test/video_player/video_collection.h"
#include "media/gpu/test/video_player/video_player.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

namespace media {
namespace test {

namespace {
// Test environment for video decode tests.
class VideoDecoderTestEnvironment : public ::testing::Environment {
 public:
  VideoDecoderTestEnvironment() {}
  virtual ~VideoDecoderTestEnvironment() {}

  // Setup up the video decode test environment, only called once.
  void SetUp() override;
  // Tear down the video decode test environment, only called once.
  void TearDown() override;

  std::unique_ptr<base::test::ScopedTaskEnvironment> task_environment_;
  std::unique_ptr<FrameRendererDummy> dummy_frame_renderer_;

  // An exit manager is required to run callbacks on shutdown.
  base::AtExitManager at_exit_manager;
};

void VideoDecoderTestEnvironment::SetUp() {
  // Setting up a task environment will create a task runner for the current
  // thread and allow posting tasks to other threads. This is required for the
  // test video player to function correctly.
  task_environment_ = std::make_unique<base::test::ScopedTaskEnvironment>(
      base::test::ScopedTaskEnvironment::MainThreadType::UI);

  // Set the default test data path.
  media::test::Video::SetTestDataPath(media::GetTestDataPath());

  dummy_frame_renderer_ = FrameRendererDummy::Create();
  ASSERT_NE(dummy_frame_renderer_, nullptr);

  // Perform all static initialization that is required when running video
  // decoders in a test environment.
#if BUILDFLAG(USE_VAAPI)
  media::VaapiWrapper::PreSandboxInitialization();
#endif
}

void VideoDecoderTestEnvironment::TearDown() {
  dummy_frame_renderer_.reset();
  task_environment_.reset();
}

media::test::VideoDecoderTestEnvironment* g_env;

}  // namespace

// TODO(dstaessens@)
// * Fetch the expected number of frames from the video file's metadata.
TEST(VideoDecodeAcceleratorTest, BasicPlayTest) {
  const Video* video = &kDefaultTestVideoCollection[0];
  auto tvp = VideoPlayer::Create(video, g_env->dummy_frame_renderer_.get());
  ASSERT_NE(tvp, nullptr);

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForEvent(VideoPlayerEvent::kFlushDone));

  EXPECT_EQ(tvp->GetEventCount(VideoPlayerEvent::kFlushDone), 1u);
  EXPECT_EQ(tvp->GetEventCount(VideoPlayerEvent::kFrameDecoded), 250u);
}

}  // namespace test
}  // namespace media

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  base::CommandLine::Init(argc, argv);

  // Using shared memory requires mojo to be initialized (crbug.com/849207).
  mojo::core::Init();

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  LOG_ASSERT(logging::InitLogging(settings));

  media::test::g_env = static_cast<media::test::VideoDecoderTestEnvironment*>(
      testing::AddGlobalTestEnvironment(
          new media::test::VideoDecoderTestEnvironment()));

  return RUN_ALL_TESTS();
}
