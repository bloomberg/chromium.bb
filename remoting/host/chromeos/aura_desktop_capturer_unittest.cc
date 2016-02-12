// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/aura_desktop_capturer.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "cc/output/copy_output_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using testing::_;
using testing::SaveArg;

namespace remoting {

namespace {

// Test frame data.
const unsigned char frame_data[] = {
    0x00, 0x00, 0x00, 0x9a, 0x65, 0x1e, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x60, 0x90,
    0x24, 0x71, 0xf8, 0xf2, 0xe5, 0xdf, 0x7f, 0x81, 0xc7, 0x49, 0xc4, 0xa3,
    0x58, 0x5c, 0xf6, 0xcc, 0x40, 0x14, 0x28, 0x0c, 0xa0, 0xfa, 0x03, 0x18,
    0x38, 0xd8, 0x7d, 0x77, 0x2b, 0x3a, 0x00, 0x00, 0x00, 0x20, 0x64, 0x46,
    0x47, 0x2f, 0xdf, 0x6e, 0xed, 0x7b, 0xf3, 0xc3, 0x37, 0x20, 0xf2, 0x36,
    0x67, 0x6c, 0x36, 0xe1, 0xb4, 0x5e, 0xbe, 0x04, 0x85, 0xdb, 0x89, 0xa3,
    0xcd, 0xfd, 0xd2, 0x4b, 0xd6, 0x9f, 0x00, 0x00, 0x00, 0x40, 0x38, 0x35,
    0x05, 0x75, 0x1d, 0x13, 0x6e, 0xb3, 0x6b, 0x1d, 0x29, 0xae, 0xd3, 0x43,
    0xe6, 0x84, 0x8f, 0xa3, 0x9d, 0x65, 0x4e, 0x2f, 0x57, 0xe3, 0xf6, 0xe6,
    0x20, 0x3c, 0x00, 0xc6, 0xe1, 0x73, 0x34, 0xe2, 0x23, 0x99, 0xc4, 0xfa,
    0x91, 0xc2, 0xd5, 0x97, 0xc1, 0x8b, 0xd0, 0x3c, 0x13, 0xba, 0xf0, 0xd7
  };
}  // namespace

class AuraDesktopCapturerTest : public testing::Test,
                                public webrtc::DesktopCapturer::Callback {
 public:
  AuraDesktopCapturerTest() {}

  void SetUp() override;

  MOCK_METHOD1(OnCaptureCompleted, void(webrtc::DesktopFrame* frame));

 protected:
  void SimulateFrameCapture() {
    scoped_ptr<SkBitmap> bitmap(new SkBitmap());
    const SkImageInfo& info =
        SkImageInfo::Make(3, 4, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
    bitmap->installPixels(info, const_cast<unsigned char*>(frame_data), 12);

    capturer_->OnFrameCaptured(
        cc::CopyOutputResult::CreateBitmapResult(std::move(bitmap)));
  }

  scoped_ptr<AuraDesktopCapturer> capturer_;
};

void AuraDesktopCapturerTest::SetUp() {
  capturer_.reset(new AuraDesktopCapturer());
}

TEST_F(AuraDesktopCapturerTest, ConvertSkBitmapToDesktopFrame) {
  webrtc::DesktopFrame* captured_frame = nullptr;

  EXPECT_CALL(*this, OnCaptureCompleted(_)).Times(1).WillOnce(
      SaveArg<0>(&captured_frame));
  capturer_->Start(this);

  SimulateFrameCapture();

  ASSERT_TRUE(captured_frame != nullptr);
  uint8_t* captured_data = captured_frame->data();
  EXPECT_EQ(
      0,
      memcmp(
          frame_data, captured_data, sizeof(frame_data)));

  delete captured_frame;
}

}  // namespace remoting
