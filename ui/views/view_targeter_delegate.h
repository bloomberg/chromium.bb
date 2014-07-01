// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_TARGETER_DELEGATE_H_
#define UI_VIEWS_VIEW_TARGETER_DELEGATE_H_

#include "base/macros.h"
#include "ui/views/views_export.h"

namespace gfx {
class Rect;
}

namespace views {
class View;

// Defines the default behaviour for hit-testing a rectangular region against
// the bounds of a View. Subclasses of View wishing to define custom
// hit-testing behaviour can extend this class.
class VIEWS_EXPORT ViewTargeterDelegate {
 public:
  ViewTargeterDelegate() {}
  virtual ~ViewTargeterDelegate() {}

  // Returns true if the bounds of |target| intersects |rect|, where |rect|
  // is in the local coodinate space of |target|. Overrides of this method by
  // a View subclass should enforce DCHECK_EQ(this, target).
  virtual bool DoesIntersectRect(const View* target,
                                 const gfx::Rect& rect) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewTargeterDelegate);
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_TARGETER_DELEGATE_H_
