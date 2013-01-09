// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/capturer/video_frame_capturer_fake.h"

#include "base/time.h"
#include "remoting/capturer/capture_data.h"

namespace remoting {

// VideoFrameCapturerFake generates a white picture of size kWidth x kHeight
// with a rectangle of size kBoxWidth x kBoxHeight. The rectangle moves kSpeed
// pixels per frame along both axes, and bounces off the sides of the screen.
static const int kWidth = VideoFrameCapturerFake::kWidth;
static const int kHeight = VideoFrameCapturerFake::kHeight;
static const int kBoxWidth = 140;
static const int kBoxHeight = 140;
static const int kSpeed = 20;

COMPILE_ASSERT(kBoxWidth < kWidth && kBoxHeight < kHeight, bad_box_size);
COMPILE_ASSERT((kBoxWidth % kSpeed == 0) && (kWidth % kSpeed == 0) &&
               (kBoxHeight % kSpeed == 0) && (kHeight % kSpeed == 0),
               sizes_must_be_multiple_of_kSpeed);

VideoFrameCapturerFake::VideoFrameCapturerFake()
    : size_(SkISize::Make(0, 0)),
      bytes_per_row_(0),
      box_pos_x_(0),
      box_pos_y_(0),
      box_speed_x_(kSpeed),
      box_speed_y_(kSpeed),
      current_buffer_(0) {
  ScreenConfigurationChanged();
}

VideoFrameCapturerFake::~VideoFrameCapturerFake() {
}

void VideoFrameCapturerFake::Start(Delegate* delegate) {
  delegate_ = delegate;
}

void VideoFrameCapturerFake::Stop() {
}

void VideoFrameCapturerFake::InvalidateRegion(const SkRegion& invalid_region) {
  helper_.InvalidateRegion(invalid_region);
}

void VideoFrameCapturerFake::CaptureFrame() {
  base::Time capture_start_time = base::Time::Now();

  GenerateImage();
  helper_.InvalidateScreen(size_);

  SkRegion invalid_region;
  helper_.SwapInvalidRegion(&invalid_region);

  current_buffer_ = (current_buffer_ + 1) % kNumBuffers;

  scoped_refptr<CaptureData> capture_data(new CaptureData(
      buffers_[current_buffer_].get(), bytes_per_row_, size_));
  capture_data->mutable_dirty_region() = invalid_region;

  helper_.set_size_most_recent(capture_data->size());

  capture_data->set_capture_time_ms(
      (base::Time::Now() - capture_start_time).InMillisecondsRoundedUp());
  delegate_->OnCaptureCompleted(capture_data);
}

const SkISize& VideoFrameCapturerFake::size_most_recent() const {
  return helper_.size_most_recent();
}

void VideoFrameCapturerFake::GenerateImage() {
  memset(buffers_[current_buffer_].get(), 0xff,
         size_.width() * size_.height() * CaptureData::kBytesPerPixel);

  uint8* row = buffers_[current_buffer_].get() +
      (box_pos_y_ * size_.width() + box_pos_x_) * CaptureData::kBytesPerPixel;

  box_pos_x_ += box_speed_x_;
  if (box_pos_x_ + kBoxWidth >= size_.width() || box_pos_x_ == 0)
    box_speed_x_ = -box_speed_x_;

  box_pos_y_ += box_speed_y_;
  if (box_pos_y_ + kBoxHeight >= size_.height() || box_pos_y_ == 0)
    box_speed_y_ = -box_speed_y_;

  // Draw rectangle with the following colors in its corners:
  //     cyan....yellow
  //     ..............
  //     blue.......red
  for (int y = 0; y < kBoxHeight; ++y) {
    for (int x = 0; x < kBoxWidth; ++x) {
      int r = x * 255 / kBoxWidth;
      int g = y * 255 / kBoxHeight;
      int b = 255 - (x * 255 / kBoxWidth);
      row[x * CaptureData::kBytesPerPixel] = r;
      row[x * CaptureData::kBytesPerPixel + 1] = g;
      row[x * CaptureData::kBytesPerPixel + 2] = b;
      row[x * CaptureData::kBytesPerPixel + 3] = 0xff;
    }
    row += bytes_per_row_;
  }
}

void VideoFrameCapturerFake::ScreenConfigurationChanged() {
  size_ = SkISize::Make(kWidth, kHeight);
  bytes_per_row_ = size_.width() * CaptureData::kBytesPerPixel;

  // Create memory for the buffers.
  int buffer_size = size_.height() * bytes_per_row_;
  for (int i = 0; i < kNumBuffers; i++) {
    buffers_[i].reset(new uint8[buffer_size]);
  }
}

}  // namespace remoting
