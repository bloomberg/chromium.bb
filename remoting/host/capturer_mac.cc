// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer_mac.h"

namespace remoting {

// TODO(dmaclach): Implement this class.
CapturerMac::CapturerMac() {
}

CapturerMac::~CapturerMac() {
}

void CapturerMac::CaptureFullScreen(Task* done_task) {
  dirty_rects_.clear();

  CaptureImage();
  dirty_rects_.push_back(gfx::Rect(width_, height_));

  FinishCapture(done_task);
}

void CapturerMac::CaptureDirtyRects(Task* done_task) {
  dirty_rects_.clear();

  CaptureImage();
  // TODO(garykac): Diff old/new images and generate |dirty_rects_|.
  // Currently, this just marks the entire screen as dirty.
  dirty_rects_.push_back(gfx::Rect(width_, height_));

  FinishCapture(done_task);
}

void CapturerMac::CaptureRect(const gfx::Rect& rect, Task* done_task) {
  dirty_rects_.clear();

  CaptureImage();
  dirty_rects_.push_back(rect);

  FinishCapture(done_task);
}

void CapturerMac::GetData(const uint8* planes[]) const {
}

void CapturerMac::GetDataStride(int strides[]) const {
}

void CapturerMac::CaptureImage() {
}

}  // namespace remoting
