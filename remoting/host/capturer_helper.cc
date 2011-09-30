// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer_helper.h"

namespace remoting {

CapturerHelper::CapturerHelper() : size_most_recent_(0, 0) {
}

CapturerHelper::~CapturerHelper() {
}

void CapturerHelper::ClearInvalidRegion() {
  base::AutoLock auto_invalid_region_lock(invalid_region_lock_);
  invalid_region_.setEmpty();
}

void CapturerHelper::InvalidateRegion(const SkRegion& invalid_region) {
  base::AutoLock auto_invalid_region_lock(invalid_region_lock_);
  invalid_region_.op(invalid_region, SkRegion::kUnion_Op);
}

void CapturerHelper::InvalidateScreen(const gfx::Size& size) {
  base::AutoLock auto_invalid_region_lock(invalid_region_lock_);
  invalid_region_.op(SkIRect::MakeWH(size.width(), size.height()),
                     SkRegion::kUnion_Op);
}

void CapturerHelper::InvalidateFullScreen() {
  if (size_most_recent_ != gfx::Size(0, 0))
    InvalidateScreen(size_most_recent_);
}

void CapturerHelper::SwapInvalidRegion(SkRegion* invalid_region) {
  base::AutoLock auto_invalid_region_lock(invalid_region_lock_);
  invalid_region->swap(invalid_region_);
}

const gfx::Size& CapturerHelper::size_most_recent() const {
  return size_most_recent_;
}

void CapturerHelper::set_size_most_recent(const gfx::Size& size) {
  size_most_recent_ = size;
}

}  // namespace remoting
