// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <stdlib.h>

#include "media/base/video_frame.h"
#include "remoting/base/base_mock_objects.h"
#include "remoting/base/codec_test.h"
#include "remoting/base/decoder.h"
#include "remoting/base/encoder.h"
#include "remoting/base/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"

static const int kWidth = 320;
static const int kHeight = 240;
static const int kBytesPerPixel = 4;

// Some sample rects for testing.
static const gfx::Rect kTestRects[] = {
  gfx::Rect(0, 0, kWidth, kHeight),
  gfx::Rect(0, 0, kWidth / 2, kHeight / 2),
  gfx::Rect(kWidth / 2, kHeight / 2, kWidth / 2, kHeight / 2),
  gfx::Rect(16, 16, 16, 16),
  gfx::Rect(128, 64, 32, 32),
};

namespace remoting {

// A class to test the message output of the encoder.
class EncoderMessageTester {
 public:
  EncoderMessageTester()
      : begin_rect_(0),
        rect_data_(0),
        end_rect_(0),
        added_rects_(0),
        state_(kWaitingForBeginRect),
        strict_(false) {
  }

  ~EncoderMessageTester() {
    EXPECT_EQ(begin_rect_, end_rect_);
    EXPECT_GT(begin_rect_, 0);
    EXPECT_EQ(kWaitingForBeginRect, state_);
    if (strict_) {
      EXPECT_EQ(added_rects_, begin_rect_);
    }
  }

  // Test that we received the correct packet.
  void ReceivedPacket(VideoPacket* packet) {
    if (state_ == kWaitingForBeginRect) {
      EXPECT_TRUE((packet->flags() & VideoPacket::FIRST_PACKET) != 0);
      state_ = kWaitingForRectData;
      ++begin_rect_;

      if (strict_) {
        gfx::Rect rect = rects_.front();
        rects_.pop_front();
        EXPECT_EQ(rect.x(), packet->format().x());
        EXPECT_EQ(rect.y(), packet->format().y());
        EXPECT_EQ(rect.width(), packet->format().width());
        EXPECT_EQ(rect.height(), packet->format().height());
      }
    } else {
      EXPECT_FALSE((packet->flags() & VideoPacket::FIRST_PACKET) != 0);
    }

    if (state_ == kWaitingForRectData) {
      if (packet->has_data()) {
        ++rect_data_;
      }

      if ((packet->flags() & VideoPacket::LAST_PACKET) != 0) {
        // Expect that we have received some data.
        EXPECT_GT(rect_data_, 0);
        rect_data_ = 0;
        state_ = kWaitingForBeginRect;
        ++end_rect_;
      }

      if ((packet->flags() & VideoPacket::LAST_PARTITION) != 0) {
        // LAST_PARTITION must always be marked with LAST_PACKET.
        EXPECT_TRUE((packet->flags() & VideoPacket::LAST_PACKET) != 0);
      }
    }
  }

  void set_strict(bool strict) {
    strict_ = strict;
  }

  void AddRects(const gfx::Rect* rects, int count) {
    rects_.insert(rects_.begin() + rects_.size(), rects, rects + count);
    added_rects_ += count;
  }

 private:
  enum State {
    kWaitingForBeginRect,
    kWaitingForRectData,
  };

  int begin_rect_;
  int rect_data_;
  int end_rect_;
  int added_rects_;
  State state_;
  bool strict_;

  std::deque<gfx::Rect> rects_;

  DISALLOW_COPY_AND_ASSIGN(EncoderMessageTester);
};

class DecoderTester {
 public:
  DecoderTester(Decoder* decoder)
      : strict_(false),
        decoder_(decoder) {
    media::VideoFrame::CreateFrame(media::VideoFrame::RGB32,
                                   kWidth, kHeight,
                                   base::TimeDelta(),
                                   base::TimeDelta(), &frame_);
    EXPECT_TRUE(frame_.get());
    decoder_->Initialize(frame_);
  }

  void Reset() {
    rects_.clear();
    update_rects_.clear();
  }

  void ReceivedPacket(VideoPacket* packet) {
    Decoder::DecodeResult result = decoder_->DecodePacket(packet);

    ASSERT_NE(Decoder::DECODE_ERROR, result);

    if (result == Decoder::DECODE_DONE) {
      decoder_->GetUpdatedRects(&update_rects_);
    }
  }

  void set_strict(bool strict) {
    strict_ = strict;
  }

  void set_capture_data(scoped_refptr<CaptureData> data) {
    capture_data_ = data;
  }

  void AddRects(const gfx::Rect* rects, int count) {
    rects_.insert(rects_.begin() + rects_.size(), rects, rects + count);
  }

  void VerifyResults() {
    if (!strict_)
      return;

    ASSERT_TRUE(capture_data_.get());

    // Test the content of the update rect.
    ASSERT_EQ(rects_.size(), update_rects_.size());
    for (size_t i = 0; i < update_rects_.size(); ++i) {
      gfx::Rect rect = rects_[i];
      EXPECT_EQ(rect, update_rects_[i]);

      EXPECT_EQ(frame_->stride(0), capture_data_->data_planes().strides[0]);
      const int stride = frame_->stride(0);
      const int offset =  stride * update_rects_[i].y() +
          kBytesPerPixel * update_rects_[i].x();
      const uint8* original = capture_data_->data_planes().data[0] + offset;
      const uint8* decoded = frame_->data(0) + offset;
      const int row_size = kBytesPerPixel * update_rects_[i].width();
      for (int y = 0; y < update_rects_[i].height(); ++y) {
        EXPECT_EQ(0, memcmp(original, decoded, row_size))
            << "Row " << y << " is different";
        original += stride;
        decoded += stride;
      }
    }
  }

 private:
  bool strict_;
  std::deque<gfx::Rect> rects_;
  UpdatedRects update_rects_;
  Decoder* decoder_;
  scoped_refptr<media::VideoFrame> frame_;
  scoped_refptr<CaptureData> capture_data_;

  DISALLOW_COPY_AND_ASSIGN(DecoderTester);
};

// The EncoderTester provides a hook for retrieving the data, and passing the
// message to other subprograms for validaton.
class EncoderTester {
 public:
  EncoderTester(EncoderMessageTester* message_tester)
      : message_tester_(message_tester),
        decoder_tester_(NULL),
        data_available_(0) {
  }

  ~EncoderTester() {
    EXPECT_GT(data_available_, 0);
  }

  void DataAvailable(VideoPacket *packet) {
    ++data_available_;
    message_tester_->ReceivedPacket(packet);

    // Send the message to the DecoderTester.
    if (decoder_tester_) {
      decoder_tester_->ReceivedPacket(packet);
    }

    delete packet;
  }

  void AddRects(const gfx::Rect* rects, int count) {
    message_tester_->AddRects(rects, count);
  }

  void set_decoder_tester(DecoderTester* decoder_tester) {
    decoder_tester_ = decoder_tester;
  }

 private:
  EncoderMessageTester* message_tester_;
  DecoderTester* decoder_tester_;
  int data_available_;

  DISALLOW_COPY_AND_ASSIGN(EncoderTester);
};

scoped_refptr<CaptureData> PrepareEncodeData(media::VideoFrame::Format format,
                                             uint8** memory) {
  // TODO(hclam): Support also YUV format.
  CHECK_EQ(format, media::VideoFrame::RGB32);
  int size = kWidth * kHeight * kBytesPerPixel;

  *memory = new uint8[size];
  srand(0);
  for (int i = 0; i < size; ++i) {
    (*memory)[i] = rand() % 256;
  }

  DataPlanes planes;
  memset(planes.data, 0, sizeof(planes.data));
  memset(planes.strides, 0, sizeof(planes.strides));
  planes.data[0] = *memory;
  planes.strides[0] = kWidth * kBytesPerPixel;

  scoped_refptr<CaptureData> data =
      new CaptureData(planes, gfx::Size(kWidth, kHeight), format);
  return data;
}

static void TestEncodingRects(Encoder* encoder,
                              EncoderTester* tester,
                              scoped_refptr<CaptureData> data,
                              const gfx::Rect* rects, int count) {
  data->mutable_dirty_rects().clear();
  for (int i = 0; i < count; ++i) {
    data->mutable_dirty_rects().insert(rects[i]);
  }
  tester->AddRects(rects, count);

  encoder->Encode(data, true,
                  NewCallback(tester, &EncoderTester::DataAvailable));
}

void TestEncoder(Encoder* encoder, bool strict) {
  EncoderMessageTester message_tester;
  message_tester.set_strict(strict);

  EncoderTester tester(&message_tester);

  uint8* memory;
  scoped_refptr<CaptureData> data =
      PrepareEncodeData(media::VideoFrame::RGB32, &memory);

  TestEncodingRects(encoder, &tester, data, kTestRects, 1);
  TestEncodingRects(encoder, &tester, data, kTestRects + 1, 1);
  TestEncodingRects(encoder, &tester, data, kTestRects + 2, 1);
  TestEncodingRects(encoder, &tester, data, kTestRects + 3, 2);
  delete [] memory;
}

static void TestEncodingRects(Encoder* encoder,
                              EncoderTester* encoder_tester,
                              DecoderTester* decoder_tester,
                              scoped_refptr<CaptureData> data,
                              const gfx::Rect* rects, int count) {
  data->mutable_dirty_rects().clear();
  for (int i = 0; i < count; ++i) {
    data->mutable_dirty_rects().insert(rects[i]);
  }
  encoder_tester->AddRects(rects, count);
  decoder_tester->AddRects(rects, count);

  // Generate random data for the updated rects.
  srand(0);
  for (int i = 0; i < count; ++i) {
    const gfx::Rect rect = rects[i];
    const int bytes_per_pixel = GetBytesPerPixel(data->pixel_format());
    const int row_size = bytes_per_pixel * rect.width();
    uint8* memory = data->data_planes().data[0] +
                    data->data_planes().strides[0] * rect.y() +
                    bytes_per_pixel * rect.x();
    for (int y = 0; y < rect.height(); ++y) {
      for (int x = 0; x < row_size; ++x)
        memory[x] = rand() % 256;
      memory += data->data_planes().strides[0];
    }
  }

  encoder->Encode(data, true,
                  NewCallback(encoder_tester, &EncoderTester::DataAvailable));
  decoder_tester->VerifyResults();
  decoder_tester->Reset();
}

void TestEncoderDecoder(Encoder* encoder, Decoder* decoder, bool strict) {
  EncoderMessageTester message_tester;
  message_tester.set_strict(strict);

  EncoderTester encoder_tester(&message_tester);

  uint8* memory;
  scoped_refptr<CaptureData> data =
      PrepareEncodeData(media::VideoFrame::RGB32, &memory);
  DecoderTester decoder_tester(decoder);
  decoder_tester.set_strict(strict);
  decoder_tester.set_capture_data(data);
  encoder_tester.set_decoder_tester(&decoder_tester);

  TestEncodingRects(encoder, &encoder_tester, &decoder_tester, data,
                    kTestRects, 1);
  TestEncodingRects(encoder, &encoder_tester, &decoder_tester, data,
                    kTestRects + 1, 1);
  TestEncodingRects(encoder, &encoder_tester, &decoder_tester, data,
                    kTestRects + 2, 1);
  TestEncodingRects(encoder, &encoder_tester, &decoder_tester, data,
                    kTestRects + 3, 2);
  delete [] memory;
}

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::DecoderTester);
