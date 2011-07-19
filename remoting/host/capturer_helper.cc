// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer_helper.h"

#include <algorithm>
#include <iterator>

namespace remoting {

CapturerHelper::CapturerHelper() : size_most_recent_(0, 0) {
}

CapturerHelper::~CapturerHelper() {
}

void CapturerHelper::ClearInvalidRects() {
  base::AutoLock auto_inval_rects_lock(inval_rects_lock_);
  inval_rects_.clear();
}

void CapturerHelper::InvalidateRects(const InvalidRects& inval_rects) {
  base::AutoLock auto_inval_rects_lock(inval_rects_lock_);
  InvalidRects temp_rects;
  std::set_union(inval_rects_.begin(), inval_rects_.end(),
                 inval_rects.begin(), inval_rects.end(),
                 std::inserter(temp_rects, temp_rects.begin()));
  inval_rects_.swap(temp_rects);
}

void CapturerHelper::InvalidateScreen(const gfx::Size& size) {
  base::AutoLock auto_inval_rects_lock(inval_rects_lock_);
  inval_rects_.clear();
  inval_rects_.insert(gfx::Rect(0, 0, size.width(), size.height()));
}

void CapturerHelper::InvalidateFullScreen() {
  if (size_most_recent_ != gfx::Size(0, 0))
    InvalidateScreen(size_most_recent_);
}

bool CapturerHelper::IsCaptureFullScreen(const gfx::Size& size) {
  base::AutoLock auto_inval_rects_lock(inval_rects_lock_);
  return inval_rects_.size() == 1u &&
      inval_rects_.begin()->x() == 0 && inval_rects_.begin()->y() == 0 &&
      inval_rects_.begin()->width() == size.width() &&
      inval_rects_.begin()->height() == size.height();
}

void CapturerHelper::SwapInvalidRects(InvalidRects& inval_rects) {
  base::AutoLock auto_inval_rects_lock(inval_rects_lock_);
  inval_rects.swap(inval_rects_);
}

const gfx::Size& CapturerHelper::size_most_recent() const {
  return size_most_recent_;
}

void CapturerHelper::set_size_most_recent(const gfx::Size& size) {
  size_most_recent_ = size;
}

}  // namespace remoting
