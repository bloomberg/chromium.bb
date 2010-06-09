// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer_fake.h"

#include "gfx/rect.h"

namespace remoting {

static const int kWidth = 320;
static const int kHeight = 240;
static const int kBytesPerPixel = 4;  // 32 bit RGB is 4 bytes per pixel.
static const int kMaxColorChannelValue = 255;

CapturerFake::CapturerFake()
    : seed_(0) {
  // Dimensions of screen.
  width_ = kWidth;
  height_ = kHeight;
  pixel_format_ = PixelFormatRgb32;
  bytes_per_pixel_ = kBytesPerPixel;
  bytes_per_row_ = width_ * bytes_per_pixel_;

  // Create memory for the buffers.
  int buffer_size = height_ * bytes_per_row_;
  for (int i = 0; i < kNumBuffers; i++) {
    buffers_[i].reset(new uint8[buffer_size]);
  }
}

CapturerFake::~CapturerFake() {
}

void CapturerFake::CaptureFullScreen(Task* done_task) {
  dirty_rects_.clear();

  GenerateImage();
  dirty_rects_.push_back(gfx::Rect(width_, height_));

  FinishCapture(done_task);
}

void CapturerFake::CaptureDirtyRects(Task* done_task) {
  dirty_rects_.clear();

  GenerateImage();
  // TODO(garykac): Diff old/new images and generate |dirty_rects_|.
  // Currently, this just marks the entire screen as dirty.
  dirty_rects_.push_back(gfx::Rect(width_, height_));

  FinishCapture(done_task);
}

void CapturerFake::CaptureRect(const gfx::Rect& rect, Task* done_task) {
  dirty_rects_.clear();

  GenerateImage();
  dirty_rects_.push_back(rect);

  FinishCapture(done_task);
}

void CapturerFake::GetData(const uint8* planes[]) const {
  planes[0] = buffers_[current_buffer_].get();
  planes[1] = planes[2] = NULL;
}

void CapturerFake::GetDataStride(int strides[]) const {
  // Only the first plane has data.
  strides[0] = bytes_per_row_;
  strides[1] = strides[2] = 0;
}

void CapturerFake::GenerateImage() {
  uint8* row = buffers_[current_buffer_].get();
  for (int y = 0; y < height_; ++y) {
    int offset = y % 3;
    for (int x = 0; x < width_; ++x) {
      row[x * kBytesPerPixel + offset] = seed_++;
      seed_ &= kMaxColorChannelValue;
    }
    row += bytes_per_row_;
  }
  ++seed_;
}

}  // namespace remoting
