// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_helper.h"

#include "base/memory/scoped_ptr.h"
#include "remoting/proto/video.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using webrtc::BasicDesktopFrame;
using webrtc::DesktopRect;
using webrtc::DesktopRegion;
using webrtc::DesktopSize;
using webrtc::DesktopVector;

namespace remoting {

TEST(VideoEncoderHelperTest, PropagatesCommonFields) {
  BasicDesktopFrame frame(DesktopSize(32, 32));
  frame.set_dpi(DesktopVector(96, 97));
  frame.set_capture_time_ms(20);
  frame.mutable_updated_region()->SetRect(DesktopRect::MakeLTRB(0, 0, 16, 16));
  scoped_ptr<DesktopRegion> shape(
      new DesktopRegion(DesktopRect::MakeLTRB(16, 0, 32, 16)));
  frame.set_shape(shape.release());

  VideoEncoderHelper helper;
  scoped_ptr<VideoPacket> packet(helper.CreateVideoPacket(frame));

  ASSERT_TRUE(packet->has_format());
  EXPECT_FALSE(packet->format().has_encoding());
  EXPECT_TRUE(packet->format().has_screen_width());
  EXPECT_TRUE(packet->format().has_screen_height());
  EXPECT_TRUE(packet->format().has_x_dpi());
  EXPECT_TRUE(packet->format().has_y_dpi());

  EXPECT_TRUE(packet->has_capture_time_ms());
  EXPECT_EQ(1, packet->dirty_rects().size());

  ASSERT_TRUE(packet->has_use_desktop_shape());
  EXPECT_TRUE(packet->use_desktop_shape());

  EXPECT_EQ(1, packet->desktop_shape_rects().size());
}

TEST(VideoEncoderHelperTest, ZeroDpi) {
  BasicDesktopFrame frame(DesktopSize(32, 32));
  // DPI is zero unless explicitly set.

  VideoEncoderHelper helper;
  scoped_ptr<VideoPacket> packet(helper.CreateVideoPacket(frame));

  // Packet should have a format containing the screen dimensions only.
  ASSERT_TRUE(packet->has_format());
  EXPECT_TRUE(packet->format().has_screen_width());
  EXPECT_TRUE(packet->format().has_screen_height());
  EXPECT_FALSE(packet->format().has_x_dpi());
  EXPECT_FALSE(packet->format().has_y_dpi());
}

TEST(VideoEncoderHelperTest, NoShape) {
  BasicDesktopFrame frame(DesktopSize(32, 32));

  VideoEncoderHelper helper;
  scoped_ptr<VideoPacket> packet(helper.CreateVideoPacket(frame));

  EXPECT_FALSE(packet->use_desktop_shape());
  EXPECT_EQ(0, packet->desktop_shape_rects().size());
}

TEST(VideoEncoderHelperTest, NoScreenSizeIfUnchanged) {
  BasicDesktopFrame frame(DesktopSize(32, 32));
  // Set DPI so that the packet will have a format, with DPI but no size.
  frame.set_dpi(DesktopVector(96, 97));

  VideoEncoderHelper helper;
  scoped_ptr<VideoPacket> packet(helper.CreateVideoPacket(frame));
  packet = helper.CreateVideoPacket(frame);

  ASSERT_TRUE(packet->has_format());
  EXPECT_FALSE(packet->format().has_screen_width());
  EXPECT_FALSE(packet->format().has_screen_height());
  EXPECT_TRUE(packet->format().has_x_dpi());
  EXPECT_TRUE(packet->format().has_y_dpi());
}

TEST(VideoEncoderHelperTest, ScreenSizeWhenChanged) {
  VideoEncoderHelper helper;

  // Process the same frame twice, so the helper knows the current size, and
  // to trigger suppression of the size field due to the size not changing.
  BasicDesktopFrame frame1(DesktopSize(32, 32));
  scoped_ptr<VideoPacket> packet(helper.CreateVideoPacket(frame1));
  packet = helper.CreateVideoPacket(frame1);

  // Process a different-sized frame to trigger size to be emitted.
  BasicDesktopFrame frame2(DesktopSize(48, 48));
  packet = helper.CreateVideoPacket(frame2);

  ASSERT_TRUE(packet->has_format());
  EXPECT_TRUE(packet->format().has_screen_width());
  EXPECT_TRUE(packet->format().has_screen_height());
}

}  // namespace remoting
