// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer.h"

#include <algorithm>

#include "remoting/base/tracer.h"

namespace remoting {

Capturer::Capturer(MessageLoop* message_loop)
    : width_(0),
      height_(0),
      pixel_format_(media::VideoFrame::INVALID),
      bytes_per_row_(0),
      current_buffer_(0),
      message_loop_(message_loop) {
}

Capturer::~Capturer() {
}

// Return the width of the screen.
int Capturer::width() const {
  return width_;
}

// Return the height of the screen.
int Capturer::height() const {
  return height_;
}

// Return the pixel format of the screen.
media::VideoFrame::Format Capturer::pixel_format() const {
  return pixel_format_;
}

void Capturer::ClearInvalidRects() {
  base::AutoLock auto_inval_rects_lock(inval_rects_lock_);
  inval_rects_.clear();
}

void Capturer::InvalidateRects(const InvalidRects& inval_rects) {
  InvalidRects temp_rects;
  std::set_union(inval_rects_.begin(), inval_rects_.end(),
                 inval_rects.begin(), inval_rects.end(),
                 std::inserter(temp_rects, temp_rects.begin()));
  {
    base::AutoLock auto_inval_rects_lock(inval_rects_lock_);
    inval_rects_.swap(temp_rects);
  }
}

void Capturer::InvalidateFullScreen() {
  base::AutoLock auto_inval_rects_lock(inval_rects_lock_);
  inval_rects_.clear();
  inval_rects_.insert(gfx::Rect(0, 0, width_, height_));
}

void Capturer::CaptureInvalidRects(CaptureCompletedCallback* callback) {
  // Calculate which rects need to be captured.
  TraceContext::tracer()->PrintString("Started CalculateInvalidRects");
  CalculateInvalidRects();
  TraceContext::tracer()->PrintString("Done CalculateInvalidRects");

  // Braced to scope the lock.
  InvalidRects local_rects;
  {
    base::AutoLock auto_inval_rects_lock(inval_rects_lock_);
    local_rects.swap(inval_rects_);
  }

  TraceContext::tracer()->PrintString("Start CaptureRects");
  CaptureRects(local_rects, callback);
}

void Capturer::FinishCapture(scoped_refptr<CaptureData> data,
                             CaptureCompletedCallback* callback) {
  // Select the next buffer to be the current buffer.
  current_buffer_ = (current_buffer_ + 1) % kNumBuffers;

  callback->Run(data);
  delete callback;
}

}  // namespace remoting
