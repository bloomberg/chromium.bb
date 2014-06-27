// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/video_frame_recorder.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace webrtc {

// Define equality operator for DesktopFrame to allow use of EXPECT_EQ().
static bool operator==(const DesktopFrame& a,
                       const DesktopFrame& b) {
  if ((a.size().equals(b.size())) &&
      (a.updated_region().Equals(b.updated_region())) &&
      (a.dpi().equals(b.dpi()))) {
    for (int i = 0; i < a.size().height(); ++i) {
      if (memcmp(a.data() + a.stride() * i,
                 b.data() + b.stride() * i,
                 a.size().width() * DesktopFrame::kBytesPerPixel) != 0) {
        return false;
      }
    }
    return true;
  }
  return false;
}

} // namespace

namespace remoting {

const int64_t kMaxContentBytes = 10 * 1024 * 1024;
const int kWidth = 640;
const int kHeight = 480;
const int kTestFrameCount = 6;

class VideoFrameRecorderTest : public testing::Test {
 public:
  VideoFrameRecorderTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void CreateAndWrapEncoder();
  scoped_ptr<webrtc::DesktopFrame> CreateNextFrame();
  void CreateTestFrames();
  void EncodeTestFrames();
  void EncodeDummyFrame();
  void StartRecording();
  void VerifyTestFrames();

 protected:
  base::MessageLoop message_loop_;

  scoped_ptr<VideoFrameRecorder> recorder_;
  scoped_ptr<VideoEncoder> encoder_;

  std::list<webrtc::DesktopFrame*> test_frames_;
  int frame_count_;
};

VideoFrameRecorderTest::VideoFrameRecorderTest() : frame_count_(0) {}

void VideoFrameRecorderTest::SetUp() {
  recorder_.reset(new VideoFrameRecorder());
  recorder_->SetMaxContentBytes(kMaxContentBytes);
}

void VideoFrameRecorderTest::TearDown() {
  ASSERT_TRUE(test_frames_.empty());

  // Allow events posted to the recorder_, if still valid, to be processed.
  base::RunLoop().RunUntilIdle();

  // Tear down the recorder, if necessary.
  recorder_.reset();

  // Process any events resulting from recorder teardown.
  base::RunLoop().RunUntilIdle();
}

void VideoFrameRecorderTest::CreateAndWrapEncoder() {
  scoped_ptr<VideoEncoder> encoder(new VideoEncoderVerbatim());
  encoder_ = recorder_->WrapVideoEncoder(encoder.Pass());

  // Encode a dummy frame to bind the wrapper to the TaskRunner.
  EncodeDummyFrame();
}

scoped_ptr<webrtc::DesktopFrame> VideoFrameRecorderTest::CreateNextFrame() {
  scoped_ptr<webrtc::DesktopFrame> frame(
      new webrtc::BasicDesktopFrame(webrtc::DesktopSize(kWidth, kHeight)));

  // Fill content, DPI and updated-region based on |frame_count_| so that each
  // generated frame is different.
  memset(frame->data(), frame_count_, frame->stride() * kHeight);
  frame->set_dpi(webrtc::DesktopVector(frame_count_, frame_count_));
  frame->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeWH(frame_count_, frame_count_));
  ++frame_count_;

  return frame.Pass();
}

void VideoFrameRecorderTest::CreateTestFrames() {
  for (int i=0; i < kTestFrameCount; ++i) {
    test_frames_.push_back(CreateNextFrame().release());
  }
}

void VideoFrameRecorderTest::EncodeTestFrames() {
  std::list<webrtc::DesktopFrame*>::iterator i;
  for (i = test_frames_.begin(); i != test_frames_.end(); ++i) {
    scoped_ptr<VideoPacket> packet = encoder_->Encode(*(*i));

    // Process tasks to let the recorder pick up the frame.
    base::RunLoop().RunUntilIdle();
  }
}

void VideoFrameRecorderTest::EncodeDummyFrame() {
  webrtc::BasicDesktopFrame dummy_frame(webrtc::DesktopSize(kWidth, kHeight));
  scoped_ptr<VideoPacket> packet = encoder_->Encode(dummy_frame);
  base::RunLoop().RunUntilIdle();
}

void VideoFrameRecorderTest::StartRecording() {
  // Start the recorder and pump events to let things initialize.
  recorder_->SetEnableRecording(true);
  base::RunLoop().RunUntilIdle();
}

void VideoFrameRecorderTest::VerifyTestFrames() {
  // Verify that the recorded frames match the ones passed to the encoder.
  while (!test_frames_.empty()) {
    scoped_ptr<webrtc::DesktopFrame> recorded_frame(recorder_->NextFrame());
    ASSERT_TRUE(recorded_frame);

    scoped_ptr<webrtc::DesktopFrame> expected_frame(test_frames_.front());
    test_frames_.pop_front();

    EXPECT_EQ(*recorded_frame, *expected_frame);
  }

  EXPECT_FALSE(recorder_->NextFrame());
}

// Basic test that creating & tearing down VideoFrameRecorder doesn't crash.
TEST_F(VideoFrameRecorderTest, CreateDestroy) {
}

// Basic test that creating, starting, stopping and destroying a
// VideoFrameRecorder don't end the world.
TEST_F(VideoFrameRecorderTest, StartStop) {
  StartRecording();
  recorder_->SetEnableRecording(false);
}

// Test that tearing down the VideoFrameRecorder while the VideoEncoder
// wrapper exists doesn't crash.
TEST_F(VideoFrameRecorderTest, DestroyVideoFrameRecorderFirst) {
  CreateAndWrapEncoder();

  // Start the recorder, so that the wrapper will push frames to it.
  StartRecording();

  // Tear down the recorder.
  recorder_.reset();

  // Encode a dummy frame via the wrapper to ensure we don't crash.
  EncodeDummyFrame();
}

// Test that creating & tearing down the wrapper while the
// VideoFrameRecorder still exists doesn't crash.
TEST_F(VideoFrameRecorderTest, DestroyVideoEncoderWrapperFirst) {
  CreateAndWrapEncoder();

  // Start the recorder, so that the wrapper will push frames to it.
  StartRecording();

  // Encode a dummy frame via the wrapper to ensure we don't crash.
  EncodeDummyFrame();

  // Tear down the encoder wrapper.
  encoder_.reset();

  // Test teardown will stop the recorder and process pending events.
}

// Test that when asked to encode a short sequence of frames, those frames are
// all recorded, in sequence.
TEST_F(VideoFrameRecorderTest, RecordFrames) {
  CreateAndWrapEncoder();

  // Start the recorder, so that the wrapper will push frames to it.
  StartRecording();

  // Create frames, store them and pass them to the encoder.
  CreateTestFrames();
  EncodeTestFrames();

  // Verify that the recorded frames match the ones passed to the encoder.
  VerifyTestFrames();
}

// Test that when asked to record more frames than the maximum content bytes
// limit allows, the first encoded frames are dropped.
TEST_F(VideoFrameRecorderTest, MaxContentBytesEnforced) {
  CreateAndWrapEncoder();

  // Configure a maximum content size sufficient for five and a half frames.
  int64 frame_bytes = kWidth * kHeight * webrtc::DesktopFrame::kBytesPerPixel;
  recorder_->SetMaxContentBytes((frame_bytes * 11) / 2);

  // Start the recorder, so that the wrapper will push frames to it.
  StartRecording();

  // Create frames, store them and pass them to the encoder.
  CreateTestFrames();
  EncodeTestFrames();

  // Only five of the supplied frames should have been recorded.
  while (test_frames_.size() > 5) {
    scoped_ptr<webrtc::DesktopFrame> frame(test_frames_.front());
    test_frames_.pop_front();
  }

  // Verify that the recorded frames match the ones passed to the encoder.
  VerifyTestFrames();
}

// Test that when asked to record more frames than the maximum content bytes
// limit allows, the first encoded frames are dropped.
TEST_F(VideoFrameRecorderTest, ContentBytesUpdatedByNextFrame) {
  CreateAndWrapEncoder();

  // Configure a maximum content size sufficient for kTestFrameCount frames.
  int64 frame_bytes = kWidth * kHeight * webrtc::DesktopFrame::kBytesPerPixel;
  recorder_->SetMaxContentBytes(frame_bytes * kTestFrameCount);

  // Start the recorder, so that the wrapper will push frames to it.
  StartRecording();

  // Encode a frame, to record it, and consume it from the recorder.
  EncodeDummyFrame();
  scoped_ptr<webrtc::DesktopFrame> frame = recorder_->NextFrame();
  EXPECT_TRUE(frame);

  // Create frames, store them and pass them to the encoder.
  CreateTestFrames();
  EncodeTestFrames();

  // Verify that the recorded frames match the ones passed to the encoder.
  VerifyTestFrames();
}

// Test that when asked to encode a short sequence of frames, none are recorded
// if recording was not enabled.
TEST_F(VideoFrameRecorderTest, EncodeButDontRecord) {
  CreateAndWrapEncoder();

  // Create frames, store them and pass them to the encoder.
  CreateTestFrames();
  EncodeTestFrames();

  // Clear the list of expected test frames, since none should be recorded.
  STLDeleteElements(&test_frames_);

  // Verify that the recorded frames match the ones passed to the encoder.
  VerifyTestFrames();
}

}  // namespace remoting
