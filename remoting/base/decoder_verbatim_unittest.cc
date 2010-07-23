// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "remoting/base/decoder_verbatim.h"
#include "remoting/client/mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;

namespace remoting {

TEST(DecoderVerbatimTest, SimpleDecode) {
  DecoderVerbatim decoder;
  scoped_refptr<MockDecodeDoneHandler> handler = new MockDecodeDoneHandler();

  const size_t kWidth = 10;
  const size_t kHeight = 1;
  const char kData[] = "ABCDEFGHIJ";
  scoped_ptr<HostMessage> msg(new HostMessage());

  // Prepare the begin rect message.
  UpdateStreamBeginRect* begin_rect =
      msg->mutable_update_stream_packet()->mutable_begin_rect();
  begin_rect->set_width(kWidth);
  begin_rect->set_height(kHeight);
  begin_rect->set_x(0);
  begin_rect->set_y(0);
  begin_rect->set_pixel_format(PixelFormatAscii);

  // Prepare the rect data.
  msg->mutable_update_stream_packet()->mutable_rect_data()->set_data(
      kData, sizeof(kData));

  // Prepare the end rect.
  msg->mutable_update_stream_packet()->mutable_end_rect();

  scoped_refptr<media::VideoFrame> frame;
  media::VideoFrame::CreateFrame(media::VideoFrame::ASCII, kWidth, kHeight,
                                 base::TimeDelta(), base::TimeDelta(), &frame);
  ASSERT_TRUE(frame);

  InSequence s;
  EXPECT_CALL(*handler, PartialDecodeDone());
  EXPECT_CALL(*handler, DecodeDone());

  UpdatedRects rects;
  decoder.BeginDecode(
      frame, &rects,
      NewRunnableMethod(handler.get(),
                        &MockDecodeDoneHandler::PartialDecodeDone),
      NewRunnableMethod(handler.get(), &MockDecodeDoneHandler::DecodeDone));
  decoder.PartialDecode(msg.release());
  decoder.EndDecode();

  // Make sure we get the same data back.
  EXPECT_EQ(kWidth, frame->width());
  EXPECT_EQ(kHeight, frame->height());
  EXPECT_EQ(media::VideoFrame::ASCII, frame->format());
  // TODO(hclam): Enable this line.
  // EXPECT_EQ(0, memcmp(kData, frame->data(media::VideoFrame::kRGBPlane),
  //                     sizeof(kData)));

  // Check the updated rects.
  ASSERT_TRUE(rects.size() == 1);
  EXPECT_EQ(kWidth, static_cast<size_t>(rects[0].width()));
  EXPECT_EQ(kHeight, static_cast<size_t>(rects[0].height()));
}

}  // namespace remoting
