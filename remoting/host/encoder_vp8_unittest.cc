// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/data_buffer.h"
#include "remoting/base/pixel_format.h"
#include "remoting/host/encoder_vp8.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

static const int kWidth = 1024;
static const int kHeight = 768;
static const PixelFormat kPixelFormat = kPixelFormat_YV12;

static void GenerateData(uint8* data, int size) {
  for (int i = 0; i < size; ++i) {
    data[i] = i;
  }
}

class EncodeDoneHandler
    : public base::RefCountedThreadSafe<EncodeDoneHandler> {
 public:
  MOCK_METHOD0(EncodeDone, void());
};

TEST(EncoderVp8Test, SimpleEncode) {
  EncoderVp8 encoder;
  encoder.SetSize(kWidth, kHeight);
  encoder.SetPixelFormat(kPixelFormat);

  DirtyRects rects;
  rects.push_back(gfx::Rect(kWidth, kHeight));

  // Prepare memory for encoding.
  int strides[3];
  strides[0] = kWidth;
  strides[1] = strides[2] = kWidth / 2;

  uint8* planes[3];
  planes[0] = new uint8[kWidth * kHeight];
  planes[1] = new uint8[kWidth * kHeight / 4];
  planes[2] = new uint8[kWidth * kHeight / 4];
  GenerateData(planes[0], kWidth * kHeight);
  GenerateData(planes[1], kWidth * kHeight / 4);
  GenerateData(planes[2], kWidth * kHeight / 4);

  scoped_refptr<EncodeDoneHandler> handler = new EncodeDoneHandler();
  UpdateStreamPacketHeader* header = new UpdateStreamPacketHeader();
  scoped_refptr<media::DataBuffer> encoded_data;
  bool encode_done = false;
  EXPECT_CALL(*handler, EncodeDone());
  encoder.Encode(rects, const_cast<const uint8**>(planes),
                 strides, true, header, &encoded_data, &encode_done,
                 NewRunnableMethod(handler.get(),
                                   &EncodeDoneHandler::EncodeDone));

  EXPECT_TRUE(encode_done);
  ASSERT_TRUE(encoded_data.get());
  EXPECT_NE(0u, encoded_data->GetBufferSize());

  delete [] planes[0];
  delete [] planes[1];
  delete [] planes[2];
}

}  // namespace remoting
