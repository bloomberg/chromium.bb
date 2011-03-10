// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer_fake_ascii.h"

#include "ui/gfx/rect.h"

namespace remoting {

static const int kWidth = 32;
static const int kHeight = 20;
static const int kBytesPerPixel = 1;

CapturerFakeAscii::CapturerFakeAscii(MessageLoop* message_loop)
    : Capturer(message_loop) {
}

CapturerFakeAscii::~CapturerFakeAscii() {
}

void CapturerFakeAscii::ScreenConfigurationChanged() {
  width_ = kWidth;
  height_ = kHeight;
  bytes_per_row_ = width_ * kBytesPerPixel;
  pixel_format_ = media::VideoFrame::ASCII;

  // Create memory for the buffers.
  int buffer_size = height_ * bytes_per_row_;
  for (int i = 0; i < kNumBuffers; i++) {
    buffers_[i].reset(new uint8[buffer_size]);
  }
}

void CapturerFakeAscii::CalculateInvalidRects() {
  // Capture and invalidate the entire screen.
  // Performing the capture here is modelled on the Windows
  // GDI capturer.
  GenerateImage();
  InvalidateFullScreen();
}

void CapturerFakeAscii::CaptureRects(const InvalidRects& rects,
                                     CaptureCompletedCallback* callback) {
  DataPlanes planes;
  planes.data[0] = buffers_[current_buffer_].get();
  planes.strides[0] = bytes_per_row_;
  scoped_refptr<CaptureData> capture_data(new CaptureData(
      planes, gfx::Size(width_, height_), pixel_format_));
  FinishCapture(capture_data, callback);
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
