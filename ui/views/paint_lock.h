// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_PAINT_LOCK_H_
#define UI_VIEWS_PAINT_LOCK_H_
#pragma once

#include "base/basictypes.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// Instances of PaintLock can be created to disable painting of the view
// (compositing is not disabled). When the class is destroyed, painting is
// re-enabled. This can be useful during operations like animations, that are
// sensitive to costly paints, and during which only composting, not painting,
// is required.
class VIEWS_EXPORT PaintLock {
 public:
  // The paint lock does not own the view. It is an error for the view to be
  // destroyed before the lock.
  explicit PaintLock(View* view);
  ~PaintLock();

 private:
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(PaintLock);
};

} // namespace views

#endif  // UI_VIEWS_PAINT_LOCK_H_
