// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer_linux.h"

namespace remoting {

// TODO(dmaclach): Implement this class.
CapturerLinux::CapturerLinux() {
}

CapturerLinux::~CapturerLinux() {
}

void CapturerLinux::CaptureFullScreen(Task* done_task) {
  dirty_rects_.clear();

  CaptureImage();
  dirty_rects_.push_back(gfx::Rect(width_, height_));

  FinishCapture(done_task);
}

void CapturerLinux::CaptureDirtyRects(Task* done_task) {
  dirty_rects_.clear();

  CaptureImage();
  // TODO(garykac): Diff old/new images and generate |dirty_rects_|.
  // Currently, this just marks the entire screen as dirty.
  dirty_rects_.push_back(gfx::Rect(width_, height_));

  FinishCapture(done_task);
}

void CapturerLinux::CaptureRect(const gfx::Rect& rect, Task* done_task) {
  dirty_rects_.clear();

  CaptureImage();
  dirty_rects_.push_back(rect);

  FinishCapture(done_task);
}

void CapturerLinux::GetData(const uint8* planes[]) const {
}

void CapturerLinux::GetDataStride(int strides[]) const {
}

void CapturerLinux::CaptureImage() {
}

}  // namespace remoting
