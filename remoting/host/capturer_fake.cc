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
}

CapturerFake::~CapturerFake() {
}

void CapturerFake::CaptureRects(const RectVector& rects,
                                CaptureCompletedCallback* callback) {
  GenerateImage();
  Capturer::DataPlanes planes;
  planes.data[0] = buffers_[current_buffer_].get();
  planes.strides[0] = bytes_per_row_;

  scoped_refptr<CaptureData> capture_data(new CaptureData(planes,
                                                          width_,
                                                          height_,
                                                          pixel_format_));
  capture_data->mutable_dirty_rects() = rects;
  FinishCapture(capture_data, callback);
}

void CapturerFake::ScreenConfigurationChanged() {
  width_ = kWidth;
  height_ = kHeight;
  pixel_format_ = PixelFormatRgb32;
  bytes_per_row_ = width_ * kBytesPerPixel;

  // Create memory for the buffers.
  int buffer_size = height_ * bytes_per_row_;
  for (int i = 0; i < kNumBuffers; i++) {
    buffers_[i].reset(new uint8[buffer_size]);
  }
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
