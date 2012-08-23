// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <stdlib.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "remoting/codec/codec_test.h"
#include "remoting/codec/video_decoder.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/base/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kBytesPerPixel = 4;

// Some sample rects for testing.
std::vector<std::vector<SkIRect> > MakeTestRectLists(const SkISize& size) {
  std::vector<std::vector<SkIRect> > rect_lists;
  std::vector<SkIRect> rects;
  rects.push_back(SkIRect::MakeXYWH(0, 0, size.width(), size.height()));
  rect_lists.push_back(rects);
  rects.clear();
  rects.push_back(SkIRect::MakeXYWH(0, 0, size.width() / 2, size.height() / 2));
  rect_lists.push_back(rects);
  rects.clear();
  rects.push_back(SkIRect::MakeXYWH(size.width() / 2, size.height() / 2,
                                    size.width() / 2, size.height() / 2));
  rect_lists.push_back(rects);
  rects.clear();
  rects.push_back(SkIRect::MakeXYWH(16, 16, 16, 16));
  rects.push_back(SkIRect::MakeXYWH(128, 64, 32, 32));
  rect_lists.push_back(rects);
  return rect_lists;
}

}  // namespace

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
        SkIRect rect = rects_.front();
        rects_.pop_front();
        EXPECT_EQ(rect.fLeft, packet->format().x());
        EXPECT_EQ(rect.fTop, packet->format().y());
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

  void AddRects(const SkIRect* rects, int count) {
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

  std::deque<SkIRect> rects_;

  DISALLOW_COPY_AND_ASSIGN(EncoderMessageTester);
};

class DecoderTester {
 public:
  DecoderTester(Decoder* decoder, const SkISize& screen_size,
                const SkISize& view_size)
      : screen_size_(screen_size),
        view_size_(view_size),
        strict_(false),
        decoder_(decoder) {
    image_data_.reset(new uint8[
        view_size_.width() * view_size_.height() * kBytesPerPixel]);
    EXPECT_TRUE(image_data_.get());
    decoder_->Initialize(screen_size_);
  }

  void Reset() {
    expected_region_.setEmpty();
    update_region_.setEmpty();
  }

  void ResetRenderedData() {
    memset(image_data_.get(), 0,
           view_size_.width() * view_size_.height() * kBytesPerPixel);
  }

  void ReceivedPacket(VideoPacket* packet) {
    Decoder::DecodeResult result = decoder_->DecodePacket(packet);

    ASSERT_NE(Decoder::DECODE_ERROR, result);

    if (result == Decoder::DECODE_DONE) {
      RenderFrame();
    }
  }

  void RenderFrame() {
    decoder_->RenderFrame(view_size_,
                          SkIRect::MakeSize(view_size_),
                          image_data_.get(),
                          view_size_.width() * kBytesPerPixel,
                          &update_region_);
  }

  void ReceivedScopedPacket(scoped_ptr<VideoPacket> packet) {
    ReceivedPacket(packet.get());
  }

  void set_strict(bool strict) {
    strict_ = strict;
  }

  void set_capture_data(scoped_refptr<CaptureData> data) {
    capture_data_ = data;
  }

  void AddRects(const SkIRect* rects, int count) {
    SkRegion new_rects;
    new_rects.setRects(rects, count);
    AddRegion(new_rects);
  }

  void AddRegion(const SkRegion& region) {
    expected_region_.op(region, SkRegion::kUnion_Op);
  }

  void VerifyResults() {
    if (!strict_)
      return;

    ASSERT_TRUE(capture_data_.get());

    // Test the content of the update region.
    EXPECT_EQ(expected_region_, update_region_);
    for (SkRegion::Iterator i(update_region_); !i.done(); i.next()) {
      const int stride = view_size_.width() * kBytesPerPixel;
      EXPECT_EQ(stride, capture_data_->data_planes().strides[0]);
      const int offset =  stride * i.rect().top() +
          kBytesPerPixel * i.rect().left();
      const uint8* original = capture_data_->data_planes().data[0] + offset;
      const uint8* decoded = image_data_.get() + offset;
      const int row_size = kBytesPerPixel * i.rect().width();
      for (int y = 0; y < i.rect().height(); ++y) {
        EXPECT_EQ(0, memcmp(original, decoded, row_size))
            << "Row " << y << " is different";
        original += stride;
        decoded += stride;
      }
    }
  }

  // The error at each pixel is the root mean square of the errors in
  // the R, G, and B components, each normalized to [0, 1]. This routine
  // checks that the maximum and mean pixel errors do not exceed given limits.
void VerifyResultsApprox(const uint8* expected_view_data,
                         double max_error_limit, double mean_error_limit) {
    double max_error = 0.0;
    double sum_error = 0.0;
    int error_num = 0;
    for (SkRegion::Iterator i(update_region_); !i.done(); i.next()) {
      const int stride = view_size_.width() * kBytesPerPixel;
      const int offset =  stride * i.rect().top() +
          kBytesPerPixel * i.rect().left();
      const uint8* expected = expected_view_data + offset;
      const uint8* actual = image_data_.get() + offset;
      for (int y = 0; y < i.rect().height(); ++y) {
        for (int x = 0; x < i.rect().width(); ++x) {
          double error = CalculateError(expected, actual);
          max_error = std::max(max_error, error);
          sum_error += error;
          ++error_num;
          expected += 4;
          actual += 4;
        }
      }
    }
    EXPECT_LE(max_error, max_error_limit);
    double mean_error = sum_error / error_num;
    EXPECT_LE(mean_error, mean_error_limit);
    LOG(INFO) << "Max error: " << max_error;
    LOG(INFO) << "Mean error: " << mean_error;
  }

  double CalculateError(const uint8* original, const uint8* decoded) {
    double error_sum_squares = 0.0;
    for (int i = 0; i < 3; i++) {
      double error = static_cast<double>(*original++) -
                     static_cast<double>(*decoded++);
      error /= 255.0;
      error_sum_squares += error * error;
    }
    original++;
    decoded++;
    return sqrt(error_sum_squares / 3.0);
  }

 private:
  SkISize screen_size_;
  SkISize view_size_;
  bool strict_;
  SkRegion expected_region_;
  SkRegion update_region_;
  Decoder* decoder_;
  scoped_array<uint8> image_data_;
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

  void DataAvailable(scoped_ptr<VideoPacket> packet) {
    ++data_available_;
    message_tester_->ReceivedPacket(packet.get());

    // Send the message to the DecoderTester.
    if (decoder_tester_) {
      decoder_tester_->ReceivedPacket(packet.get());
    }
  }

  void AddRects(const SkIRect* rects, int count) {
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

scoped_refptr<CaptureData> PrepareEncodeData(const SkISize& size,
                                             media::VideoFrame::Format format,
                                             uint8** memory) {
  // TODO(hclam): Support also YUV format.
  CHECK_EQ(format, media::VideoFrame::RGB32);
  int memory_size = size.width() * size.height() * kBytesPerPixel;

  *memory = new uint8[memory_size];
  srand(0);
  for (int i = 0; i < memory_size; ++i) {
    (*memory)[i] = rand() % 256;
  }

  DataPlanes planes;
  memset(planes.data, 0, sizeof(planes.data));
  memset(planes.strides, 0, sizeof(planes.strides));
  planes.data[0] = *memory;
  planes.strides[0] = size.width() * kBytesPerPixel;

  scoped_refptr<CaptureData> data =
      new CaptureData(planes, size, format);
  return data;
}

static void TestEncodingRects(Encoder* encoder,
                              EncoderTester* tester,
                              scoped_refptr<CaptureData> data,
                              const SkIRect* rects, int count) {
  data->mutable_dirty_region().setEmpty();
  for (int i = 0; i < count; ++i) {
    data->mutable_dirty_region().op(rects[i], SkRegion::kUnion_Op);
  }
  tester->AddRects(rects, count);

  encoder->Encode(data, true, base::Bind(
      &EncoderTester::DataAvailable, base::Unretained(tester)));
}

void TestEncoder(Encoder* encoder, bool strict) {
  SkISize kSize = SkISize::Make(320, 240);

  EncoderMessageTester message_tester;
  message_tester.set_strict(strict);

  EncoderTester tester(&message_tester);

  uint8* memory;
  scoped_refptr<CaptureData> data =
      PrepareEncodeData(kSize, media::VideoFrame::RGB32, &memory);
  scoped_array<uint8> memory_wrapper(memory);

  std::vector<std::vector<SkIRect> > test_rect_lists = MakeTestRectLists(kSize);
  for (size_t i = 0; i < test_rect_lists.size(); ++i) {
    const std::vector<SkIRect>& test_rects = test_rect_lists[i];
    TestEncodingRects(encoder, &tester, data,
                      &test_rects[0], test_rects.size());
  }
}

static void TestEncodeDecodeRects(Encoder* encoder,
                                  EncoderTester* encoder_tester,
                                  DecoderTester* decoder_tester,
                                  scoped_refptr<CaptureData> data,
                                  const SkIRect* rects, int count) {
  data->mutable_dirty_region().setRects(rects, count);
  encoder_tester->AddRects(rects, count);
  decoder_tester->AddRects(rects, count);

  // Generate random data for the updated region.
  srand(0);
  for (int i = 0; i < count; ++i) {
    CHECK_EQ(data->pixel_format(), media::VideoFrame::RGB32);
    const int bytes_per_pixel = 4;  // Because of RGB32 on previous line.
    const int row_size = bytes_per_pixel * rects[i].width();
    uint8* memory = data->data_planes().data[0] +
      data->data_planes().strides[0] * rects[i].top() +
      bytes_per_pixel * rects[i].left();
    for (int y = 0; y < rects[i].height(); ++y) {
      for (int x = 0; x < row_size; ++x)
        memory[x] = rand() % 256;
      memory += data->data_planes().strides[0];
    }
  }

  encoder->Encode(data, true, base::Bind(&EncoderTester::DataAvailable,
                                         base::Unretained(encoder_tester)));
  decoder_tester->VerifyResults();
  decoder_tester->Reset();
}

void TestEncoderDecoder(Encoder* encoder, Decoder* decoder, bool strict) {
  SkISize kSize = SkISize::Make(320, 240);

  EncoderMessageTester message_tester;
  message_tester.set_strict(strict);

  EncoderTester encoder_tester(&message_tester);

  uint8* memory;
  scoped_refptr<CaptureData> data =
      PrepareEncodeData(kSize, media::VideoFrame::RGB32, &memory);
  scoped_array<uint8> memory_wrapper(memory);

  DecoderTester decoder_tester(decoder, kSize, kSize);
  decoder_tester.set_strict(strict);
  decoder_tester.set_capture_data(data);
  encoder_tester.set_decoder_tester(&decoder_tester);

  std::vector<std::vector<SkIRect> > test_rect_lists = MakeTestRectLists(kSize);
  for (size_t i = 0; i < test_rect_lists.size(); ++i) {
    const std::vector<SkIRect> test_rects = test_rect_lists[i];
    TestEncodeDecodeRects(encoder, &encoder_tester, &decoder_tester, data,
                          &test_rects[0], test_rects.size());
  }
}

static void FillWithGradient(uint8* memory, const SkISize& frame_size,
                             const SkIRect& rect) {
  for (int j = rect.top(); j < rect.bottom(); ++j) {
    uint8* p = memory + ((j * frame_size.width()) + rect.left()) * 4;
    for (int i = rect.left(); i < rect.right(); ++i) {
      *p++ = static_cast<uint8>((255.0 * i) / frame_size.width());
      *p++ = static_cast<uint8>((164.0 * j) / frame_size.height());
      *p++ = static_cast<uint8>((82.0 * (i + j)) /
                                   (frame_size.width() + frame_size.height()));
      *p++ = 0;
    }
  }
}

void TestEncoderDecoderGradient(Encoder* encoder,
                                Decoder* decoder,
                                const SkISize& screen_size,
                                const SkISize& view_size,
                                double max_error_limit,
                                double mean_error_limit) {
  SkIRect screen_rect = SkIRect::MakeSize(screen_size);
  scoped_array<uint8> screen_data(new uint8[
      screen_size.width() * screen_size.height() * kBytesPerPixel]);
  FillWithGradient(screen_data.get(), screen_size, screen_rect);

  SkIRect view_rect = SkIRect::MakeSize(view_size);
  scoped_array<uint8> expected_view_data(new uint8[
      view_size.width() * view_size.height() * kBytesPerPixel]);
  FillWithGradient(expected_view_data.get(), view_size, view_rect);

  DataPlanes planes;
  memset(planes.data, 0, sizeof(planes.data));
  memset(planes.strides, 0, sizeof(planes.strides));
  planes.data[0] = screen_data.get();
  planes.strides[0] = screen_size.width() * kBytesPerPixel;

  scoped_refptr<CaptureData> capture_data =
      new CaptureData(planes, screen_size, media::VideoFrame::RGB32);
  capture_data->mutable_dirty_region().op(screen_rect, SkRegion::kUnion_Op);

  DecoderTester decoder_tester(decoder, screen_size, view_size);
  decoder_tester.set_capture_data(capture_data);
  decoder_tester.AddRegion(capture_data->dirty_region());

  encoder->Encode(capture_data, true,
                  base::Bind(&DecoderTester::ReceivedScopedPacket,
                             base::Unretained(&decoder_tester)));

  decoder_tester.VerifyResultsApprox(expected_view_data.get(),
                                     max_error_limit, mean_error_limit);

  // Check that the decoder correctly re-renders the frame if its client
  // invalidates the frame.
  decoder_tester.ResetRenderedData();
  decoder->Invalidate(view_size, SkRegion(view_rect));
  decoder_tester.RenderFrame();
  decoder_tester.VerifyResultsApprox(expected_view_data.get(),
                                     max_error_limit, mean_error_limit);
}

}  // namespace remoting
