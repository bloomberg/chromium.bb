// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <stdlib.h>

#include "gfx/rect.h"
#include "media/base/video_frame.h"
#include "remoting/base/codec_test.h"
#include "remoting/base/encoder.h"
#include "remoting/base/mock_objects.h"
#include "remoting/base/protocol_util.h"
#include "testing/gtest/include/gtest/gtest.h"

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

// A class to test that the state transition of the Encoder is correct.
class EncoderStateTester {
 public:
  EncoderStateTester()
      : next_state_(Encoder::EncodingStarting) {
  }

  ~EncoderStateTester() {
    EXPECT_EQ(Encoder::EncodingStarting, next_state_);
  }

  // Set the state output of the Encoder.
  void ReceivedState(Encoder::EncodingState state) {
    if (state & Encoder::EncodingStarting) {
      EXPECT_EQ(Encoder::EncodingStarting, next_state_);
      next_state_ = Encoder::EncodingInProgress | Encoder::EncodingEnded;
    } else {
      EXPECT_FALSE(next_state_ & Encoder::EncodingStarting);
    }

    if (state & Encoder::EncodingInProgress) {
      EXPECT_TRUE(next_state_ & Encoder::EncodingInProgress);
    }

    if (state & Encoder::EncodingEnded) {
      EXPECT_TRUE(next_state_ & Encoder::EncodingEnded);
      next_state_ = Encoder::EncodingStarting;
    }
  }

 private:
  Encoder::EncodingState next_state_;

  DISALLOW_COPY_AND_ASSIGN(EncoderStateTester);
};

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

  // Test that we received the correct message.
  void ReceivedMessage(ChromotingHostMessage* message) {
    EXPECT_TRUE(message->has_update_stream_packet());

    if (state_ == kWaitingForBeginRect) {
      EXPECT_TRUE(message->update_stream_packet().has_begin_rect());
      state_ = kWaitingForRectData;
      ++begin_rect_;

      if (strict_) {
        gfx::Rect rect = rects_.front();
        rects_.pop_front();
        EXPECT_EQ(rect.x(), message->update_stream_packet().begin_rect().x());
        EXPECT_EQ(rect.y(), message->update_stream_packet().begin_rect().y());
        EXPECT_EQ(rect.width(),
                  message->update_stream_packet().begin_rect().width());
        EXPECT_EQ(rect.height(),
                  message->update_stream_packet().begin_rect().height());
      }
    } else {
      EXPECT_FALSE(message->update_stream_packet().has_begin_rect());
    }

    if (state_ == kWaitingForRectData) {
      if (message->update_stream_packet().has_rect_data()) {
        ++rect_data_;
      }

      if (message->update_stream_packet().has_end_rect()) {
        // Expect that we have received some data.
        EXPECT_GT(rect_data_, 0);
        rect_data_ = 0;
        state_ = kWaitingForBeginRect;
        ++end_rect_;
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
        decoder_(decoder),
        decode_done_(false) {
    media::VideoFrame::CreateFrame(media::VideoFrame::RGB32,
                                   kWidth, kHeight,
                                   base::TimeDelta(),
                                   base::TimeDelta(), &frame_);
    EXPECT_TRUE(frame_.get());
  }

  void ReceivedMessage(ChromotingHostMessage* message) {
    if (message->has_update_stream_packet()) {
      decoder_->PartialDecode(message);
      return;
    }

    if (message->has_begin_update_stream()) {
      decoder_->BeginDecode(
          frame_, &update_rects_,
          NewRunnableMethod(this, &DecoderTester::OnPartialDecodeDone),
          NewRunnableMethod(this, &DecoderTester::OnDecodeDone));
    }

    if (message->has_end_update_stream()) {
      decoder_->EndDecode();
    }
    delete message;
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

  bool decode_done() const { return decode_done_; }
  void reset_decode_done() { decode_done_ = false; }

 private:
  void OnPartialDecodeDone() {
    if (!strict_)
      return;

    // Test the content of the update rect.
    for (size_t i = 0; i < update_rects_.size(); ++i) {
      EXPECT_FALSE(rects_.empty());
      gfx::Rect rect = rects_.front();
      rects_.pop_front();
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

  void OnDecodeDone() {
    decode_done_ = true;
    if (!strict_)
      return;

    EXPECT_TRUE(capture_data_.get());
    for (int i = 0; i < DataPlanes::kPlaneCount; ++i) {
      if (!frame_->data(i) || !capture_data_->data_planes().data[i])
        continue;
      // TODO(hclam): HAndle YUV.
      int size = capture_data_->data_planes().strides[i] * kHeight;
      EXPECT_EQ(0, memcmp(capture_data_->data_planes().data[i],
                          frame_->data(i), size));
    }
  }

  bool strict_;
  std::deque<gfx::Rect> rects_;
  UpdatedRects update_rects_;
  Decoder* decoder_;
  scoped_refptr<media::VideoFrame> frame_;
  scoped_refptr<CaptureData> capture_data_;
  bool decode_done_;

  DISALLOW_COPY_AND_ASSIGN(DecoderTester);
};

class EncoderTester {
 public:
  EncoderTester(EncoderMessageTester* message_tester,
                EncoderStateTester* state_tester)
      : message_tester_(message_tester),
        state_tester_(state_tester),
        decoder_tester_(NULL),
        data_available_(0) {
  }

  ~EncoderTester() {
    EXPECT_GT(data_available_, 0);
  }

  void DataAvailable(ChromotingHostMessage* message,
                     Encoder::EncodingState state) {
    ++data_available_;
    message_tester_->ReceivedMessage(message);
    state_tester_->ReceivedState(state);

    // Send the message to the DecoderTester.
    if (decoder_tester_) {
      if (state & Encoder::EncodingStarting) {
        ChromotingHostMessage* begin_update = new ChromotingHostMessage();
        begin_update->mutable_begin_update_stream();
        decoder_tester_->ReceivedMessage(begin_update);
      }

      if (state & Encoder::EncodingInProgress) {
        decoder_tester_->ReceivedMessage(message);
      }

      if (state & Encoder::EncodingEnded) {
        ChromotingHostMessage* end_update = new ChromotingHostMessage();
        end_update->mutable_end_update_stream();
        decoder_tester_->ReceivedMessage(end_update);
      }
    } else {
      delete message;
    }
  }

  void AddRects(const gfx::Rect* rects, int count) {
    message_tester_->AddRects(rects, count);
  }

  void set_decoder_tester(DecoderTester* decoder_tester) {
    decoder_tester_ = decoder_tester;
  }

 private:
  EncoderMessageTester* message_tester_;
  EncoderStateTester* state_tester_;
  DecoderTester* decoder_tester_;
  int data_available_;

  DISALLOW_COPY_AND_ASSIGN(EncoderTester);
};

scoped_refptr<CaptureData> PrepareEncodeData(PixelFormat format,
                                             uint8** memory) {
  // TODO(hclam): Support also YUV format.
  CHECK(format == PixelFormatRgb32);
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
      new CaptureData(planes, kWidth, kHeight, format);
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

  EncoderStateTester state_tester;
  EncoderTester tester(&message_tester, &state_tester);

  uint8* memory;
  scoped_refptr<CaptureData> data =
      PrepareEncodeData(PixelFormatRgb32, &memory);

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
  decoder_tester->reset_decode_done();

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
  EXPECT_TRUE(decoder_tester->decode_done());
}

void TestEncoderDecoder(Encoder* encoder, Decoder* decoder, bool strict) {
  EncoderMessageTester message_tester;
  message_tester.set_strict(strict);

  EncoderStateTester state_tester;
  EncoderTester encoder_tester(&message_tester, &state_tester);

  uint8* memory;
  scoped_refptr<CaptureData> data =
      PrepareEncodeData(PixelFormatRgb32, &memory);
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
