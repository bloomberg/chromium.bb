// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer_fake_ascii.h"

#include "gfx/rect.h"

namespace remoting {

static const int kWidth = 32;
static const int kHeight = 20;
static const int kBytesPerPixel = 1;

CapturerFakeAscii::CapturerFakeAscii() {
  // Dimensions of screen.
  width_ = kWidth;
  height_ = kHeight;
  pixel_format_ = PixelFormatAscii;
  bytes_per_pixel_ = kBytesPerPixel;
  bytes_per_row_ = width_ * bytes_per_pixel_;

  // Create memory for the buffers.
  int buffer_size = height_ * bytes_per_row_;
  for (int i = 0; i < kNumBuffers; i++) {
    buffers_[i].reset(new uint8[buffer_size]);
  }
}

CapturerFakeAscii::~CapturerFakeAscii() {
}

void CapturerFakeAscii::CaptureFullScreen(Task* done_task) {
  dirty_rects_.clear();

  GenerateImage();

  // Return a single dirty rect that includes the entire screen.
  dirty_rects_.push_back(gfx::Rect(width_, height_));

  FinishCapture(done_task);
}

void CapturerFakeAscii::CaptureDirtyRects(Task* done_task) {
  dirty_rects_.clear();

  GenerateImage();
  // TODO(garykac): Diff old/new screen.
  // Currently, this just marks the entire screen as dirty.
  dirty_rects_.push_back(gfx::Rect(width_, height_));

  FinishCapture(done_task);
}

void CapturerFakeAscii::CaptureRect(const gfx::Rect& rect, Task* done_task) {
  dirty_rects_.clear();

  GenerateImage();
  dirty_rects_.push_back(rect);

  FinishCapture(done_task);
}

void CapturerFakeAscii::GetData(const uint8* planes[]) const {
  planes[0] = buffers_[current_buffer_].get();
  planes[1] = planes[2] = NULL;
}

void CapturerFakeAscii::GetDataStride(int strides[]) const {
  // Only the first plane has data.
  strides[0] = bytes_per_row_;
  strides[1] = strides[2] = 0;
}

void CapturerFakeAscii::GenerateImage() {
  for (int y = 0; y < height_; ++y) {
    uint8* row = buffers_[current_buffer_].get() + bytes_per_row_ * y;
    for (int x = 0; x < bytes_per_row_; ++x) {
      if (y == 0 || x == 0 || x == (width_ - 1) || y == (height_ - 1)) {
        row[x] = '*';
      } else {
        row[x] = ' ';
      }
    }
  }
}

}  // namespace remoting
