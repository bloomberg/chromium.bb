// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer_gdi.h"
#include "remoting/host/differ.h"

#include "ui/gfx/rect.h"

namespace remoting {

// 3780 pixels per meter is equivalent to 96 DPI, typical on desktop monitors.
static const int kPixelsPerMeter = 3780;
// 32 bit RGBA is 4 bytes per pixel.
static const int kBytesPerPixel = 4;

CapturerGdi::CapturerGdi(MessageLoop* message_loop)
    : Capturer(message_loop),
      desktop_dc_(NULL),
      memory_dc_(NULL),
      capture_fullscreen_(true) {
  memset(target_bitmap_, 0, sizeof(target_bitmap_));
  memset(buffers_, 0, sizeof(buffers_));
  ScreenConfigurationChanged();
}

CapturerGdi::~CapturerGdi() {
  ReleaseBuffers();
}

void CapturerGdi::ReleaseBuffers() {
  for (int i = kNumBuffers - 1; i >= 0; i--) {
    if (target_bitmap_[i]) {
      DeleteObject(target_bitmap_[i]);
      target_bitmap_[i] = NULL;
    }
    if (buffers_[i]) {
      DeleteObject(buffers_[i]);
      buffers_[i] = NULL;
    }
  }

  desktop_dc_ = NULL;
  if (memory_dc_) {
    DeleteDC(memory_dc_);
    memory_dc_ = NULL;
  }
}

void CapturerGdi::ScreenConfigurationChanged() {
  ReleaseBuffers();

  desktop_dc_ = GetDC(GetDesktopWindow());
  memory_dc_ = CreateCompatibleDC(desktop_dc_);

  // Create a bitmap to keep the desktop image.
  width_ = GetSystemMetrics(SM_CXSCREEN);
  height_ = GetSystemMetrics(SM_CYSCREEN);
  int rounded_width = (width_ + 3) & (~3);

  // Dimensions of screen.
  pixel_format_ = media::VideoFrame::RGB32;
  bytes_per_row_ = rounded_width * kBytesPerPixel;

  // Create a differ for this screen size.
  differ_.reset(new Differ(width_, height_, 4, bytes_per_row_));

  // Create a device independant bitmap (DIB) that is the same size.
  BITMAPINFO bmi;
  memset(&bmi, 0, sizeof(bmi));
  bmi.bmiHeader.biHeight = height_;
  bmi.bmiHeader.biWidth = width_;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = kBytesPerPixel * 8;
  bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
  bmi.bmiHeader.biSizeImage = bytes_per_row_ * height_;
  bmi.bmiHeader.biXPelsPerMeter = kPixelsPerMeter;
  bmi.bmiHeader.biYPelsPerMeter = kPixelsPerMeter;

  // Create memory for the buffers.
  for (int i = 0; i < kNumBuffers; i++) {
    target_bitmap_[i] = CreateDIBSection(desktop_dc_, &bmi, DIB_RGB_COLORS,
                                         static_cast<void**>(&buffers_[i]),
                                         NULL, 0);
  }

  capture_fullscreen_ = true;
}

void CapturerGdi::CalculateInvalidRects() {
  ClearInvalidRects();
  CaptureImage();

  if (capture_fullscreen_) {
    InvalidateFullScreen();
  } else {
    // Calculate the difference between the previous and current screen.
    int prev_buffer_id = current_buffer_ - 1;
    if (prev_buffer_id < 0) {
      prev_buffer_id = kNumBuffers - 1;
    }

    void* prev_buffer = buffers_[prev_buffer_id];
    void* curr_buffer = buffers_[current_buffer_];

    InvalidRects rects;
    differ_->CalcDirtyRects(prev_buffer, curr_buffer, &rects);

    InvalidateRects(rects);
  }

  capture_fullscreen_ = false;
}

void CapturerGdi::CaptureRects(const InvalidRects& rects,
                               CaptureCompletedCallback* callback) {
  DataPlanes planes;
  planes.data[0] = static_cast<uint8*>(buffers_[current_buffer_]);
  planes.strides[0] = bytes_per_row_;

  scoped_refptr<CaptureData> data(new CaptureData(planes,
                                                  width(),
                                                  height(),
                                                  pixel_format()));
  data->mutable_dirty_rects() = rects;

  FinishCapture(data, callback);
}

void CapturerGdi::CaptureImage() {
  // Select the target bitmap into the memory dc.
  SelectObject(memory_dc_, target_bitmap_[current_buffer_]);

  // And then copy the rect from desktop to memory.
  BitBlt(memory_dc_, 0, 0, width_, height_, desktop_dc_, 0, 0,
      SRCCOPY | CAPTUREBLT);
}

// static
Capturer* Capturer::Create(MessageLoop* message_loop) {
  return new CapturerGdi(message_loop);
}

}  // namespace remoting
