// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer_fake.h"

#include "ui/gfx/rect.h"

namespace remoting {

// CapturerFake generates a white picture of size kWidth x kHeight with a
// rectangle of size kBoxWidth x kBoxHeight. The rectangle moves kSpeed pixels
// per frame along both axes, and bounces off the sides of the screen.
static const int kWidth = 800;
static const int kHeight = 600;
static const int kBoxWidth = 140;
static const int kBoxHeight = 140;
static const int kSpeed = 20;

COMPILE_ASSERT(kBoxWidth < kWidth && kBoxHeight < kHeight, bad_box_size);
COMPILE_ASSERT((kBoxWidth % kSpeed == 0) && (kWidth % kSpeed == 0) &&
               (kBoxHeight % kSpeed == 0) && (kHeight % kSpeed == 0),
               sizes_must_be_multiple_of_kSpeed);

static const int kBytesPerPixel = 4;  // 32 bit RGB is 4 bytes per pixel.

CapturerFake::CapturerFake(MessageLoop* message_loop)
    : Capturer(message_loop),
      box_pos_x_(0),
      box_pos_y_(0),
      box_speed_x_(kSpeed),
      box_speed_y_(kSpeed) {
  ScreenConfigurationChanged();
}

CapturerFake::~CapturerFake() {
}

void CapturerFake::ScreenConfigurationChanged() {
  width_ = kWidth;
  height_ = kHeight;
  pixel_format_ = media::VideoFrame::RGB32;
  bytes_per_row_ = width_ * kBytesPerPixel;

  // Create memory for the buffers.
  int buffer_size = height_ * bytes_per_row_;
  for (int i = 0; i < kNumBuffers; i++) {
    buffers_[i].reset(new uint8[buffer_size]);
  }
}

void CapturerFake::CalculateInvalidRects() {
  GenerateImage();
  InvalidateFullScreen();
}

void CapturerFake::CaptureRects(const InvalidRects& rects,
                                CaptureCompletedCallback* callback) {
  DataPlanes planes;
  planes.data[0] = buffers_[current_buffer_].get();
  planes.strides[0] = bytes_per_row_;

  scoped_refptr<CaptureData> capture_data(new CaptureData(planes,
                                                          width_,
                                                          height_,
                                                          pixel_format_));
  capture_data->mutable_dirty_rects() = rects;
  FinishCapture(capture_data, callback);
}

void CapturerFake::GenerateImage() {
  memset(buffers_[current_buffer_].get(), 0xff,
         width_ * height_ * kBytesPerPixel);

  uint8* row = buffers_[current_buffer_].get() +
      (box_pos_y_ * width_ + box_pos_x_) * kBytesPerPixel;

  box_pos_x_ += box_speed_x_;
  if (box_pos_x_ + kBoxWidth >= width_ || box_pos_x_ == 0)
    box_speed_x_ = -box_speed_x_;

  box_pos_y_ += box_speed_y_;
  if (box_pos_y_ + kBoxHeight >= height_ || box_pos_y_ == 0)
    box_speed_y_ = -box_speed_y_;

  // Draw rectangle with the following colors in it's corners:
  //     cyan....yellow
  //     ..............
  //     blue.......red
  for (int y = 0; y < kBoxHeight; ++y) {
    for (int x = 0; x < kBoxWidth; ++x) {
      int r = x * 255 / kBoxWidth;
      int g = y * 255 / kBoxHeight;
      int b = 255 - (x * 255 / kBoxWidth);
      row[x * kBytesPerPixel] = r;
      row[x * kBytesPerPixel+1] = g;
      row[x * kBytesPerPixel+2] = b;
      row[x * kBytesPerPixel+3] = 0xff;
    }
    row += bytes_per_row_;
  }
}

}  // namespace remoting
