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
      dc_size_(0, 0),
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
    if (buffers_[i].data) {
      DeleteObject(buffers_[i].data);
      buffers_[i].data = NULL;
    }
  }

  desktop_dc_ = NULL;
  if (memory_dc_) {
    DeleteDC(memory_dc_);
    memory_dc_ = NULL;
  }
}

void CapturerGdi::ScreenConfigurationChanged() {
  // We poll for screen configuration changes, so ignore notifications.
}

void CapturerGdi::UpdateBufferCapture(const gfx::Size& size) {
  // Make sure the DCs have the correct dimensions.
  if (size != dc_size_) {
    // TODO(simonmorris): screen dimensions changing isn't equivalent to needing
    // a new DC, but it's good enough for now.
    desktop_dc_ = GetDC(GetDesktopWindow());
    if (memory_dc_)
      DeleteDC(memory_dc_);
    memory_dc_ = CreateCompatibleDC(desktop_dc_);
    dc_size_ = size;
  }

  // Make sure the current bitmap has the correct dimensions.
  if (size != buffers_[current_buffer_].size) {
    ReallocateBuffer(current_buffer_, size);
    capture_fullscreen_ = true;
  }
}

void CapturerGdi::ReallocateBuffer(int buffer_index, const gfx::Size& size) {
  // Delete any previously constructed bitmap.
  if (target_bitmap_[buffer_index]) {
    DeleteObject(target_bitmap_[buffer_index]);
    target_bitmap_[buffer_index] = NULL;
  }
  if (buffers_[buffer_index].data) {
    DeleteObject(buffers_[buffer_index].data);
    buffers_[buffer_index].data = NULL;
  }

  // Create a bitmap to keep the desktop image.
  int rounded_width = (size.width() + 3) & (~3);

  // Dimensions of screen.
  pixel_format_ = media::VideoFrame::RGB32;
  int bytes_per_row = rounded_width * kBytesPerPixel;

  // Create a device independent bitmap (DIB) that is the same size.
  BITMAPINFO bmi;
  memset(&bmi, 0, sizeof(bmi));
  bmi.bmiHeader.biHeight = -size.height();
  bmi.bmiHeader.biWidth = size.width();
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = kBytesPerPixel * 8;
  bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
  bmi.bmiHeader.biSizeImage = bytes_per_row * size.height();
  bmi.bmiHeader.biXPelsPerMeter = kPixelsPerMeter;
  bmi.bmiHeader.biYPelsPerMeter = kPixelsPerMeter;

  // Create memory for the buffers.
  target_bitmap_[buffer_index] =
      CreateDIBSection(desktop_dc_, &bmi, DIB_RGB_COLORS,
                       static_cast<void**>(&buffers_[buffer_index].data),
                       NULL, 0);
  buffers_[buffer_index].size = gfx::Size(bmi.bmiHeader.biWidth,
                                          std::abs(bmi.bmiHeader.biHeight));
  buffers_[buffer_index].bytes_per_pixel = bmi.bmiHeader.biBitCount / 8;
  buffers_[buffer_index].bytes_per_row =
      bmi.bmiHeader.biSizeImage / std::abs(bmi.bmiHeader.biHeight);
}

void CapturerGdi::CalculateInvalidRects() {
  CaptureImage();

  const VideoFrameBuffer& current = buffers_[current_buffer_];
  if (IsCaptureFullScreen(current.size.width(), current.size.height()))
    capture_fullscreen_ = true;

  if (capture_fullscreen_) {
    InvalidateScreen(current.size);
    capture_fullscreen_ = false;
    return;
  }

  // Find the previous and current screens.
  int prev_buffer_id = current_buffer_ - 1;
  if (prev_buffer_id < 0) {
    prev_buffer_id = kNumBuffers - 1;
  }
  const VideoFrameBuffer& prev = buffers_[prev_buffer_id];

  // Maybe the previous and current screens can't be differenced.
  if ((current.size != prev.size) ||
      (current.bytes_per_pixel != prev.bytes_per_pixel) ||
      (current.bytes_per_row != prev.bytes_per_row)) {
    InvalidateScreen(current.size);
    return;
  }

  // Make sure the differencer is set up correctly for these previous and
  // current screens.
  if (!differ_.get() ||
      (differ_->width() != current.size.width()) ||
      (differ_->height() != current.size.height()) ||
      (differ_->bytes_per_pixel() != current.bytes_per_pixel) ||
      (differ_->bytes_per_row() != current.bytes_per_row)) {
    differ_.reset(new Differ(current.size.width(), current.size.height(),
      current.bytes_per_pixel, current.bytes_per_row));
  }

  InvalidRects rects;
  differ_->CalcDirtyRects(prev.data, current.data, &rects);

  InvalidateRects(rects);
}

void CapturerGdi::CaptureRects(const InvalidRects& rects,
                               CaptureCompletedCallback* callback) {
  const VideoFrameBuffer& buffer = buffers_[current_buffer_];
  DataPlanes planes;
  planes.data[0] = static_cast<uint8*>(buffer.data);
  planes.strides[0] = buffer.bytes_per_row;

  scoped_refptr<CaptureData> data(new CaptureData(planes,
                                                  buffer.size,
                                                  pixel_format_));
  data->mutable_dirty_rects() = rects;

  FinishCapture(data, callback);
}

void CapturerGdi::CaptureImage() {
  // Make sure the structures we use to capture the image have the correct size.
  UpdateBufferCapture(GetScreenSize());

  // Select the target bitmap into the memory dc.
  SelectObject(memory_dc_, target_bitmap_[current_buffer_]);

  // And then copy the rect from desktop to memory.
  BitBlt(memory_dc_, 0, 0, buffers_[current_buffer_].size.width(),
      buffers_[current_buffer_].size.height(), desktop_dc_, 0, 0,
      SRCCOPY | CAPTUREBLT);
}

gfx::Size CapturerGdi::GetScreenSize() {
  return gfx::Size(GetSystemMetrics(SM_CXSCREEN),
                   GetSystemMetrics(SM_CYSCREEN));
}

// static
Capturer* Capturer::Create(MessageLoop* message_loop) {
  return new CapturerGdi(message_loop);
}

}  // namespace remoting
