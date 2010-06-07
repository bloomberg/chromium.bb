// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer_gdi.h"

#include "gfx/rect.h"

namespace remoting {

// 3780 pixels per meter is equivalent to 96 DPI, typical on desktop monitors.
static const int kPixelsPerMeter = 3780;
// 24 bit RGB is 3 bytes per pixel.
static const int kBytesPerPixel = 3;

CapturerGdi::CapturerGdi()
    : initialized_(false) {
}

CapturerGdi::~CapturerGdi() {
  if (initialized_) {
    for (int i = kNumBuffers - 1; i >= 0; i--) {
      DeleteObject(target_bitmap_[i]);
    }
  }
}

void CapturerGdi::CaptureFullScreen(Task* done_task) {
  dirty_rects_.clear();

  CaptureImage();
  dirty_rects_.push_back(gfx::Rect(width_, height_));

  FinishCapture(done_task);
}

void CapturerGdi::CaptureDirtyRects(Task* done_task) {
  dirty_rects_.clear();

  CaptureImage();
  // TODO(garykac): Diff old/new images and generate |dirty_rects_|.
  // Currently, this just marks the entire screen as dirty.
  dirty_rects_.push_back(gfx::Rect(width_, height_));

  FinishCapture(done_task);
}

void CapturerGdi::CaptureRect(const gfx::Rect& rect, Task* done_task) {
  dirty_rects_.clear();

  CaptureImage();
  dirty_rects_.push_back(rect);

  FinishCapture(done_task);
}

void CapturerGdi::GetData(const uint8* planes[]) const {
  planes[0] = static_cast<const uint8*>(buffers_[current_buffer_]);
  planes[1] = planes[2] = NULL;
}

void CapturerGdi::GetDataStride(int strides[]) const {
  // Only the first plane has data.
  strides[0] = bytes_per_row_;
  strides[1] = strides[2] = 0;
}

int CapturerGdi::GetWidth() const {
  return GetSystemMetrics(SM_CXSCREEN);
}

int CapturerGdi::GetHeight() const {
  return GetSystemMetrics(SM_CYSCREEN);
}

// TODO(fbarchard): handle error cases.
void CapturerGdi::InitializeBuffers() {
  desktop_dc_ = GetDC(GetDesktopWindow());
  memory_dc_ = CreateCompatibleDC(desktop_dc_);

  // Create a bitmap to keep the desktop image.
  width_ = GetSystemMetrics(SM_CXSCREEN);
  height_ = GetSystemMetrics(SM_CYSCREEN);
  int rounded_width = (width_ + 3) & (~3);

  // Dimensions of screen.
  pixel_format_ = chromotocol_pb::PixelFormatRgb24;
  bytes_per_pixel_ = kBytesPerPixel;
  bytes_per_row_ = rounded_width * bytes_per_pixel_;

  // Create a device independant bitmap (DIB) that is the same size.
  BITMAPINFO bmi;
  memset(&bmi, 0, sizeof(bmi));
  bmi.bmiHeader.biHeight = height_;
  bmi.bmiHeader.biWidth = width_;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = bytes_per_pixel_ * 8;
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
  initialized_ = true;
}

void CapturerGdi::CaptureImage() {
  if (initialized_ == false) {
    InitializeBuffers();
  }
  // Selection the target bitmap into the memory dc.
  SelectObject(memory_dc_, target_bitmap_[current_buffer_]);

  // And then copy the rect from desktop to memory.
  BitBlt(memory_dc_, 0, 0, width_, height_, desktop_dc_, 0, 0,
      SRCCOPY | CAPTUREBLT);
}

}  // namespace remoting
