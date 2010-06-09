// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer.h"

namespace remoting {

Capturer::Capturer()
    : width_(0),
      height_(0),
      pixel_format_(PixelFormatInvalid),
      bytes_per_pixel_(0),
      bytes_per_row_(0),
      current_buffer_(0) {
}

Capturer::~Capturer() {
}

void Capturer::GetDirtyRects(DirtyRects* rects) const {
  *rects = dirty_rects_;
}

int Capturer::GetWidth() const {
  return width_;
}

int Capturer::GetHeight() const {
  return height_;
}

PixelFormat Capturer::GetPixelFormat() const {
  return pixel_format_;
}

void Capturer::InvalidateRect(gfx::Rect dirty_rect) {
  inval_rects_.push_back(dirty_rect);
}

void Capturer::FinishCapture(Task* done_task) {
  done_task->Run();
  delete done_task;

  // Select the next buffer to be the current buffer.
  current_buffer_ = ++current_buffer_ % kNumBuffers;
}

}  // namespace remoting
