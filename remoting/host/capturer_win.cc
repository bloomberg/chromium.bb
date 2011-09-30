// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer.h"

#include <windows.h>

#include "base/memory/scoped_ptr.h"
#include "remoting/host/capturer_helper.h"
#include "remoting/host/differ.h"

namespace remoting {

namespace {

// CapturerGdi captures 32bit RGB using GDI.
//
// CapturerGdi is double-buffered as required by Capturer. See
// remoting/host/capturer.h.
class CapturerGdi : public Capturer {
 public:
  CapturerGdi();
  virtual ~CapturerGdi();

  // Capturer interface.
  virtual void ScreenConfigurationChanged() OVERRIDE;
  virtual media::VideoFrame::Format pixel_format() const OVERRIDE;
  virtual void ClearInvalidRegion() OVERRIDE;
  virtual void InvalidateRegion(const SkRegion& invalid_region) OVERRIDE;
  virtual void InvalidateScreen(const SkISize& size) OVERRIDE;
  virtual void InvalidateFullScreen() OVERRIDE;
  virtual void CaptureInvalidRegion(CaptureCompletedCallback* callback)
      OVERRIDE;
  virtual const SkISize& size_most_recent() const OVERRIDE;

 private:
  struct VideoFrameBuffer {
    VideoFrameBuffer(void* data, const SkISize& size, int bytes_per_pixel,
                     int bytes_per_row)
      : data(data), size(size), bytes_per_pixel(bytes_per_pixel),
        bytes_per_row(bytes_per_row) {
    }
    VideoFrameBuffer() {
      data = 0;
      size = SkISize::Make(0, 0);
      bytes_per_pixel = 0;
      bytes_per_row = 0;
    }
    void* data;
    SkISize size;
    int bytes_per_pixel;
    int bytes_per_row;
  };

  // Make sure that the current buffer has the same size as the screen.
  void UpdateBufferCapture(const SkISize& size);

  // Allocate memory for a buffer of a given size, freeing any memory previously
  // allocated for that buffer.
  void ReallocateBuffer(int buffer_index, const SkISize& size);

  void CalculateInvalidRegion();
  void CaptureRegion(const SkRegion& region,
                     CaptureCompletedCallback* callback);

  void ReleaseBuffers();
  // Generates an image in the current buffer.
  void CaptureImage();

  // Gets the current screen size and calls ScreenConfigurationChanged
  // if the screen size has changed.
  void MaybeChangeScreenConfiguration();

  // Gets the screen size.
  SkISize GetScreenSize();

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  CapturerHelper helper_;

  // There are two buffers for the screen images, as required by Capturer.
  static const int kNumBuffers = 2;
  VideoFrameBuffer buffers_[kNumBuffers];

  // Gdi specific information about screen.
  HDC desktop_dc_;
  HDC memory_dc_;
  HBITMAP target_bitmap_[kNumBuffers];

  // The screen size attached to the device contexts through which the screen
  // is captured.
  SkISize dc_size_;

  // The current buffer with valid data for reading.
  int current_buffer_;

  // Format of pixels returned in buffer.
  media::VideoFrame::Format pixel_format_;

  // Class to calculate the difference between two screen bitmaps.
  scoped_ptr<Differ> differ_;

  DISALLOW_COPY_AND_ASSIGN(CapturerGdi);
};

// 3780 pixels per meter is equivalent to 96 DPI, typical on desktop monitors.
static const int kPixelsPerMeter = 3780;
// 32 bit RGBA is 4 bytes per pixel.
static const int kBytesPerPixel = 4;

CapturerGdi::CapturerGdi()
    : desktop_dc_(NULL),
      memory_dc_(NULL),
      dc_size_(SkISize::Make(0, 0)),
      current_buffer_(0),
      pixel_format_(media::VideoFrame::RGB32) {
  memset(target_bitmap_, 0, sizeof(target_bitmap_));
  memset(buffers_, 0, sizeof(buffers_));
  ScreenConfigurationChanged();
}

CapturerGdi::~CapturerGdi() {
  ReleaseBuffers();
}

media::VideoFrame::Format CapturerGdi::pixel_format() const {
  return pixel_format_;
}

void CapturerGdi::ClearInvalidRegion() {
  helper_.ClearInvalidRegion();
}

void CapturerGdi::InvalidateRegion(const SkRegion& invalid_region) {
  helper_.InvalidateRegion(invalid_region);
}

void CapturerGdi::InvalidateScreen(const SkISize& size) {
  helper_.InvalidateScreen(size);
}

void CapturerGdi::InvalidateFullScreen() {
  helper_.InvalidateFullScreen();
}

void CapturerGdi::CaptureInvalidRegion(CaptureCompletedCallback* callback) {
  CalculateInvalidRegion();
  SkRegion invalid_region;
  helper_.SwapInvalidRegion(&invalid_region);
  CaptureRegion(invalid_region, callback);
}

const SkISize& CapturerGdi::size_most_recent() const {
  return helper_.size_most_recent();
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

void CapturerGdi::UpdateBufferCapture(const SkISize& size) {
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
    InvalidateFullScreen();
  }
}

void CapturerGdi::ReallocateBuffer(int buffer_index, const SkISize& size) {
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
  buffers_[buffer_index].size = SkISize::Make(bmi.bmiHeader.biWidth,
                                              std::abs(bmi.bmiHeader.biHeight));
  buffers_[buffer_index].bytes_per_pixel = bmi.bmiHeader.biBitCount / 8;
  buffers_[buffer_index].bytes_per_row =
      bmi.bmiHeader.biSizeImage / std::abs(bmi.bmiHeader.biHeight);
}

void CapturerGdi::CalculateInvalidRegion() {
  CaptureImage();

  const VideoFrameBuffer& current = buffers_[current_buffer_];

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

  SkRegion region;
  differ_->CalcDirtyRegion(prev.data, current.data, &region);

  InvalidateRegion(region);
}

void CapturerGdi::CaptureRegion(const SkRegion& region,
                                CaptureCompletedCallback* callback) {
  scoped_ptr<CaptureCompletedCallback> callback_deleter(callback);

  const VideoFrameBuffer& buffer = buffers_[current_buffer_];
  current_buffer_ = (current_buffer_ + 1) % kNumBuffers;

  DataPlanes planes;
  planes.data[0] = static_cast<uint8*>(buffer.data);
  planes.strides[0] = buffer.bytes_per_row;

  scoped_refptr<CaptureData> data(new CaptureData(planes,
                                                  buffer.size,
                                                  pixel_format_));
  data->mutable_dirty_region() = region;

  helper_.set_size_most_recent(data->size());

  callback->Run(data);
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

SkISize CapturerGdi::GetScreenSize() {
  return SkISize::Make(GetSystemMetrics(SM_CXSCREEN),
                       GetSystemMetrics(SM_CYSCREEN));
}

}  // namespace

// static
Capturer* Capturer::Create() {
  return new CapturerGdi();
}

}  // namespace remoting
