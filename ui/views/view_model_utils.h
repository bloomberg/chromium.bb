// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_MODEL_UTILS_H_
#define UI_VIEWS_VIEW_MODEL_UTILS_H_
#pragma once

#include "base/basictypes.h"
#include "ui/views/views_export.h"

namespace views {

class View;
class ViewModel;

class VIEWS_EXPORT ViewModelUtils {
 public:
  // Sets the bounds of each view to its ideal bounds.
  static void SetViewBoundsToIdealBounds(const ViewModel& model);

  // Returns the index to move |view| to based on a x-coordinate of |x|.
  static int DetermineMoveIndex(const ViewModel& model,
                                View* view,
                                int x);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ViewModelUtils);
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_MODEL_UTILS_H_
