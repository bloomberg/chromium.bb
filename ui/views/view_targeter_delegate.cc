// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {

bool ViewTargeterDelegate::DoesIntersectRect(const View* target,
                                             const gfx::Rect& rect) const {
  return target->GetLocalBounds().Intersects(rect);
}

}  // namespace views
