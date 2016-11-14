// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/ios/bridge/frame_consumer_bridge.h"

#include <gtest/gtest.h>
#include <memory>
#include <queue>

#include "base/bind.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace {
const webrtc::DesktopSize kFrameSize(100, 100);
const webrtc::DesktopVector kDpi(100, 100);

const webrtc::DesktopRect FrameRect() {
  return webrtc::DesktopRect::MakeSize(kFrameSize);
}

webrtc::DesktopRegion FrameRegion() {
  return webrtc::DesktopRegion(FrameRect());
}

void FrameDelivery(webrtc::DesktopFrame* buffer,
                   const webrtc::DesktopRegion& region) {
  ASSERT_TRUE(buffer->size().equals(kFrameSize));
  ASSERT_TRUE(region.Equals(FrameRegion()));
};

}  // namespace

namespace remoting {

class FrameProducerTester : public FrameProducer {
 public:
  virtual ~FrameProducerTester(){};

  void DrawBuffer(webrtc::DesktopFrame* buffer) override {
    frames.push(buffer);
  };

  void InvalidateRegion(const webrtc::DesktopRegion& region) override {
    NOTIMPLEMENTED();
  };

  void RequestReturnBuffers(const base::Closure& done) override {
    // Don't have to actually return the buffers.  This function is really
    // saying don't use the references anymore, they are now invalid.
    while (!frames.empty()) {
      frames.pop();
    }
    done.Run();
  };

  void SetOutputSizeAndClip(const webrtc::DesktopSize& view_size,
                            const webrtc::DesktopRect& clip_area) override {
    viewSize = view_size;
    clipArea = clip_area;
  };

  std::queue<webrtc::DesktopFrame*> frames;
  webrtc::DesktopSize viewSize;
  webrtc::DesktopRect clipArea;
};

class FrameConsumerBridgeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    frameProducer_.reset(new FrameProducerTester());
    frameConsumer_.reset(new FrameConsumerBridge(base::Bind(&FrameDelivery)));
    frameConsumer_->Initialize(frameProducer_.get());
  }
  void TearDown() override {}

  std::unique_ptr<FrameProducerTester> frameProducer_;
  std::unique_ptr<FrameConsumerBridge> frameConsumer_;
};

TEST(FrameConsumerBridgeTest_NotInitialized, CreateAndRelease) {
  std::unique_ptr<FrameConsumerBridge> frameConsumer_(
      new FrameConsumerBridge(base::Bind(&FrameDelivery)));
  ASSERT_TRUE(frameConsumer_.get() != NULL);
  frameConsumer_.reset();
  ASSERT_TRUE(frameConsumer_.get() == NULL);
}

// TODO(nicholss): FrameConsumer has changed since last integration.
// Need to update the tests.
// TEST_F(FrameConsumerBridgeTest, ApplyBuffer) {
//   webrtc::DesktopFrame* frame = NULL;
//   ASSERT_EQ(0, frameProducer_->frames.size());
//   frameConsumer_->SetSourceSize(kFrameSize, kDpi);
//   ASSERT_EQ(1, frameProducer_->frames.size());
//
//   // Return the frame, and ensure we get it back
//   frame = frameProducer_->frames.front();
//   frameProducer_->frames.pop();
//   ASSERT_EQ(0, frameProducer_->frames.size());
//   frameConsumer_->ApplyBuffer(
//       kFrameSize, FrameRect(), frame, FrameRegion(), FrameRegion());
//   ASSERT_EQ(1, frameProducer_->frames.size());
//   ASSERT_TRUE(frame == frameProducer_->frames.front());
//   ASSERT_TRUE(frame->data() == frameProducer_->frames.front()->data());
//
//   // Change the SourceSize, we should get a new frame, but when the old frame
//   is
//   // submitted we will not get it back.
//   frameConsumer_->SetSourceSize(webrtc::DesktopSize(1, 1), kDpi);
//   ASSERT_EQ(2, frameProducer_->frames.size());
//   frame = frameProducer_->frames.front();
//   frameProducer_->frames.pop();
//   ASSERT_EQ(1, frameProducer_->frames.size());
//   frameConsumer_->ApplyBuffer(
//       kFrameSize, FrameRect(), frame, FrameRegion(), FrameRegion());
//   ASSERT_EQ(1, frameProducer_->frames.size());
// }
//
// TEST_F(FrameConsumerBridgeTest, SetSourceSize) {
//   frameConsumer_->SetSourceSize(webrtc::DesktopSize(0, 0),
//                                 webrtc::DesktopVector(0, 0));
//   ASSERT_TRUE(frameProducer_->viewSize.equals(webrtc::DesktopSize(0, 0)));
//   ASSERT_TRUE(frameProducer_->clipArea.equals(
//       webrtc::DesktopRect::MakeLTRB(0, 0, 0, 0)));
//   ASSERT_EQ(1, frameProducer_->frames.size());
//   ASSERT_TRUE(
//       frameProducer_->frames.front()->size().equals(webrtc::DesktopSize(0,
//       0)));
//
//   frameConsumer_->SetSourceSize(kFrameSize, kDpi);
//   ASSERT_TRUE(frameProducer_->viewSize.equals(kFrameSize));
//   ASSERT_TRUE(frameProducer_->clipArea.equals(FrameRect()));
//   ASSERT_EQ(2, frameProducer_->frames.size());
//   frameProducer_->frames.pop();
//   ASSERT_TRUE(frameProducer_->frames.front()->size().equals(kFrameSize));
// }

}  // namespace remoting
