// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/paint_lock.h"

#include "ui/views/view.h"

namespace views {

PaintLock::PaintLock(View* view) : view_(view) {
  view_->set_painting_enabled(false);
}

PaintLock::~PaintLock() {
  view_->set_painting_enabled(true);
}

} // namespace views
