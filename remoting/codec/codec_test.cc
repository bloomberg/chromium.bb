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
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using webrtc::BasicDesktopFrame;
using webrtc::DesktopFrame;
using webrtc::DesktopRect;
using webrtc::DesktopRegion;
using webrtc::DesktopSize;

namespace {

const int kBytesPerPixel = 4;

// Some sample rects for testing.
std::vector<std::vector<DesktopRect> > MakeTestRectLists(DesktopSize size) {
  std::vector<std::vector<DesktopRect> > rect_lists;
  std::vector<DesktopRect> rects;
  rects.push_back(DesktopRect::MakeXYWH(0, 0, size.width(), size.height()));
  rect_lists.push_back(rects);
  rects.clear();
  rects.push_back(DesktopRect::MakeXYWH(
      0, 0, size.width() / 2, size.height() / 2));
  rect_lists.push_back(rects);
  rects.clear();
  rects.push_back(DesktopRect::MakeXYWH(
      size.width() / 2, size.height() / 2,
      size.width() / 2, size.height() / 2));
  rect_lists.push_back(rects);
  rects.clear();
  rects.push_back(DesktopRect::MakeXYWH(16, 16, 16, 16));
  rects.push_back(DesktopRect::MakeXYWH(128, 64, 32, 32));
  rect_lists.push_back(rects);
  return rect_lists;
}

}  // namespace

namespace remoting {

class VideoDecoderTester {
 public:
  VideoDecoderTester(VideoDecoder* decoder,
                     const DesktopSize& screen_size,
                     const DesktopSize& view_size)
      : screen_size_(screen_size),
        view_size_(view_size),
        strict_(false),
        decoder_(decoder),
        frame_(NULL) {
    image_data_.reset(new uint8[
        view_size_.width() * view_size_.height() * kBytesPerPixel]);
    EXPECT_TRUE(image_data_.get());
    decoder_->Initialize(
        DesktopSize(screen_size_.width(), screen_size_.height()));
  }

  void Reset() {
    expected_region_.Clear();
    update_region_.Clear();
  }

  void ResetRenderedData() {
    memset(image_data_.get(), 0,
           view_size_.width() * view_size_.height() * kBytesPerPixel);
  }

  void ReceivedPacket(VideoPacket* packet) {
    ASSERT_TRUE(decoder_->DecodePacket(*packet));

    RenderFrame();
  }

  void RenderFrame() {
    decoder_->RenderFrame(
        DesktopSize(view_size_.width(), view_size_.height()),
        DesktopRect::MakeWH(view_size_.width(), view_size_.height()),
        image_data_.get(), view_size_.width() * kBytesPerPixel,
        &update_region_);
  }

  void ReceivedScopedPacket(scoped_ptr<VideoPacket> packet) {
    ReceivedPacket(packet.get());
  }

  void set_strict(bool strict) {
    strict_ = strict;
  }

  void set_frame(DesktopFrame* frame) {
    frame_ = frame;
  }

  void AddRects(const DesktopRect* rects, int count) {
    for (int i = 0; i < count; ++i) {
      expected_region_.AddRect(rects[i]);
    }
  }

  void AddRegion(const DesktopRegion& region) {
    expected_region_.AddRegion(region);
  }

  void VerifyResults() {
    if (!strict_)
      return;

    ASSERT_TRUE(frame_);

    // Test the content of the update region.
    EXPECT_TRUE(expected_region_.Equals(update_region_));

    for (DesktopRegion::Iterator i(update_region_); !i.IsAtEnd();
         i.Advance()) {
      const int stride = view_size_.width() * kBytesPerPixel;
      EXPECT_EQ(stride, frame_->stride());
      const int offset =  stride * i.rect().top() +
          kBytesPerPixel * i.rect().left();
      const uint8* original = frame_->data() + offset;
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
    for (DesktopRegion::Iterator i(update_region_); !i.IsAtEnd();
         i.Advance()) {
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
    VLOG(0) << "Max error: " << max_error;
    VLOG(0) << "Mean error: " << mean_error;
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
  DesktopSize screen_size_;
  DesktopSize view_size_;
  bool strict_;
  DesktopRegion expected_region_;
  DesktopRegion update_region_;
  VideoDecoder* decoder_;
  scoped_ptr<uint8[]> image_data_;
  DesktopFrame* frame_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderTester);
};

// The VideoEncoderTester provides a hook for retrieving the data, and passing
// the message to other subprograms for validaton.
class VideoEncoderTester {
 public:
  VideoEncoderTester()
      : decoder_tester_(NULL),
        data_available_(0) {
  }

  ~VideoEncoderTester() {
    EXPECT_GT(data_available_, 0);
  }

  void DataAvailable(scoped_ptr<VideoPacket> packet) {
    ++data_available_;
    // Send the message to the VideoDecoderTester.
    if (decoder_tester_) {
      decoder_tester_->ReceivedPacket(packet.get());
    }
  }

  void set_decoder_tester(VideoDecoderTester* decoder_tester) {
    decoder_tester_ = decoder_tester;
  }

 private:
  VideoDecoderTester* decoder_tester_;
  int data_available_;

  DISALLOW_COPY_AND_ASSIGN(VideoEncoderTester);
};

scoped_ptr<DesktopFrame> PrepareFrame(const DesktopSize& size) {
  scoped_ptr<DesktopFrame> frame(new BasicDesktopFrame(size));

  srand(0);
  int memory_size = size.width() * size.height() * kBytesPerPixel;
  for (int i = 0; i < memory_size; ++i) {
    frame->data()[i] = rand() % 256;
  }

  return frame.Pass();
}

static void TestEncodingRects(VideoEncoder* encoder,
                              VideoEncoderTester* tester,
                              webrtc::DesktopFrame* frame,
                              const DesktopRect* rects,
                              int count) {
  frame->mutable_updated_region()->Clear();
  for (int i = 0; i < count; ++i) {
    frame->mutable_updated_region()->AddRect(rects[i]);
  }

  scoped_ptr<VideoPacket> packet = encoder->Encode(*frame);
  tester->DataAvailable(packet.Pass());
}

void TestVideoEncoder(VideoEncoder* encoder, bool strict) {
  const int kSizes[] = {320, 319, 317, 150};

  VideoEncoderTester tester;

  for (size_t xi = 0; xi < arraysize(kSizes); ++xi) {
    for (size_t yi = 0; yi < arraysize(kSizes); ++yi) {
      DesktopSize size = DesktopSize(kSizes[xi], kSizes[yi]);
      scoped_ptr<webrtc::DesktopFrame> frame = PrepareFrame(size);
      std::vector<std::vector<DesktopRect> > test_rect_lists =
          MakeTestRectLists(size);
      for (size_t i = 0; i < test_rect_lists.size(); ++i) {
        const std::vector<DesktopRect>& test_rects = test_rect_lists[i];
        TestEncodingRects(encoder, &tester, frame.get(),
                          &test_rects[0], test_rects.size());
      }
    }
  }
}

static void TestEncodeDecodeRects(VideoEncoder* encoder,
                                  VideoEncoderTester* encoder_tester,
                                  VideoDecoderTester* decoder_tester,
                                  DesktopFrame* frame,
                                  const DesktopRect* rects, int count) {
  frame->mutable_updated_region()->Clear();
  for (int i = 0; i < count; ++i) {
    frame->mutable_updated_region()->AddRect(rects[i]);
  }
  decoder_tester->AddRects(rects, count);

  // Generate random data for the updated region.
  srand(0);
  for (int i = 0; i < count; ++i) {
    const int row_size =
        DesktopFrame::kBytesPerPixel * rects[i].width();
    uint8* memory = frame->data() +
      frame->stride() * rects[i].top() +
      DesktopFrame::kBytesPerPixel * rects[i].left();
    for (int y = 0; y < rects[i].height(); ++y) {
      for (int x = 0; x < row_size; ++x)
        memory[x] = rand() % 256;
      memory += frame->stride();
    }
  }

  scoped_ptr<VideoPacket> packet = encoder->Encode(*frame);
  encoder_tester->DataAvailable(packet.Pass());
  decoder_tester->VerifyResults();
  decoder_tester->Reset();
}

void TestVideoEncoderDecoder(
    VideoEncoder* encoder, VideoDecoder* decoder, bool strict) {
  DesktopSize kSize = DesktopSize(320, 240);

  VideoEncoderTester encoder_tester;

  scoped_ptr<DesktopFrame> frame = PrepareFrame(kSize);

  VideoDecoderTester decoder_tester(decoder, kSize, kSize);
  decoder_tester.set_strict(strict);
  decoder_tester.set_frame(frame.get());
  encoder_tester.set_decoder_tester(&decoder_tester);

  std::vector<std::vector<DesktopRect> > test_rect_lists =
      MakeTestRectLists(kSize);
  for (size_t i = 0; i < test_rect_lists.size(); ++i) {
    const std::vector<DesktopRect> test_rects = test_rect_lists[i];
    TestEncodeDecodeRects(encoder, &encoder_tester, &decoder_tester,
                          frame.get(), &test_rects[0], test_rects.size());
  }
}

static void FillWithGradient(DesktopFrame* frame) {
  for (int j = 0; j < frame->size().height(); ++j) {
    uint8* p = frame->data() + j * frame->stride();
    for (int i = 0; i < frame->size().width(); ++i) {
      *p++ = (255.0 * i) / frame->size().width();
      *p++ = (164.0 * j) / frame->size().height();
      *p++ = (82.0 * (i + j)) /
          (frame->size().width() + frame->size().height());
      *p++ = 0;
    }
  }
}

void TestVideoEncoderDecoderGradient(VideoEncoder* encoder,
                                     VideoDecoder* decoder,
                                     const DesktopSize& screen_size,
                                     const DesktopSize& view_size,
                                     double max_error_limit,
                                     double mean_error_limit) {
  scoped_ptr<BasicDesktopFrame> frame(
      new BasicDesktopFrame(screen_size));
  FillWithGradient(frame.get());
  frame->mutable_updated_region()->SetRect(DesktopRect::MakeSize(screen_size));

  scoped_ptr<BasicDesktopFrame> expected_result(
      new BasicDesktopFrame(view_size));
  FillWithGradient(expected_result.get());

  VideoDecoderTester decoder_tester(decoder, screen_size, view_size);
  decoder_tester.set_frame(frame.get());
  decoder_tester.AddRegion(frame->updated_region());

  scoped_ptr<VideoPacket> packet = encoder->Encode(*frame);
  decoder_tester.ReceivedScopedPacket(packet.Pass());

  decoder_tester.VerifyResultsApprox(expected_result->data(),
                                     max_error_limit, mean_error_limit);

  // Check that the decoder correctly re-renders the frame if its client
  // invalidates the frame.
  decoder_tester.ResetRenderedData();
  decoder->Invalidate(
      DesktopSize(view_size.width(), view_size.height()),
      DesktopRegion(
          DesktopRect::MakeWH(view_size.width(), view_size.height())));
  decoder_tester.RenderFrame();
  decoder_tester.VerifyResultsApprox(expected_result->data(),
                                     max_error_limit, mean_error_limit);
}

float MeasureVideoEncoderFpsWithSize(VideoEncoder* encoder,
                                     const DesktopSize& size) {
  scoped_ptr<DesktopFrame> frame(PrepareFrame(size));
  frame->mutable_updated_region()->SetRect(DesktopRect::MakeSize(size));
  std::list<DesktopFrame*> frames;
  frames.push_back(frame.get());
  return MeasureVideoEncoderFpsWithFrames(encoder, frames);
}

float MeasureVideoEncoderFpsWithFrames(VideoEncoder* encoder,
                                       const std::list<DesktopFrame*>& frames) {
  const base::TimeDelta kTestTime = base::TimeDelta::FromSeconds(1);

  // Encode some frames to "warm up" the encoder (i.e. to let it set up initial
  // structures, establish a stable working set, etc), then encode at least
  // kMinimumFrameCount frames to measure the encoder's performance.
  const int kWarmUpFrameCount = 10;
  const int kMinimumFrameCount = 10;
  base::TimeTicks start_time;
  base::TimeDelta elapsed;
  std::list<DesktopFrame*> test_frames;
  int frame_count;
  for (frame_count = 0;
       (frame_count < kMinimumFrameCount + kWarmUpFrameCount ||
            elapsed < kTestTime);
       ++frame_count) {
    if (frame_count == kWarmUpFrameCount) {
      start_time = base::TimeTicks::Now();
    }

    if (test_frames.empty()) {
      test_frames = frames;
    }
    scoped_ptr<VideoPacket> packet = encoder->Encode(*test_frames.front());
    test_frames.pop_front();

    if (frame_count >= kWarmUpFrameCount) {
      elapsed = base::TimeTicks::Now() - start_time;
    }
  }

  return (frame_count * base::TimeDelta::FromSeconds(1)) / elapsed;
}

}  // namespace remoting
