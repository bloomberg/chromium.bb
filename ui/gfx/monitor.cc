// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/monitor.h"

#include "ui/gfx/insets.h"

namespace gfx {

Monitor::Monitor() : id_(-1), device_scale_factor_(1.0) {
}

Monitor::Monitor(int id) : id_(id), device_scale_factor_(1.0) {
}

Monitor::Monitor(int id, const gfx::Rect& bounds)
    : id_(id),
      bounds_(bounds),
      work_area_(bounds),
      device_scale_factor_(1.0) {
}

Monitor::~Monitor() {
}

void Monitor::SetBoundsAndUpdateWorkArea(const gfx::Rect& bounds) {
  Insets insets(work_area_.y() - bounds_.y(),
                work_area_.x() - bounds_.x(),
                bounds_.bottom() - work_area_.bottom(),
                bounds_.right() - work_area_.right());
  bounds_ = bounds;
  UpdateWorkAreaWithInsets(insets);
}

void Monitor::SetSizeAndUpdateWorkArea(const gfx::Size& size) {
  SetBoundsAndUpdateWorkArea(gfx::Rect(bounds_.origin(), size));
}

void Monitor::UpdateWorkAreaWithInsets(const gfx::Insets& insets) {
  work_area_ = bounds_;
  work_area_.Inset(insets);
}

}  // namespace gfx
