// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/monitor.h"

namespace aura {

Monitor::Monitor() {
}

Monitor::~Monitor() {
}

gfx::Rect Monitor::GetWorkAreaBounds() const {
  // TODO(oshima): For m19, work area/monitor bounds has (0,0) origin
  // because it's simpler and enough. Fix this when real multi monitor
  // support is implemented.
  gfx::Rect bounds(bounds_.size());
  bounds.Inset(work_area_insets_);
  return bounds;
}

}  // namespace aura
