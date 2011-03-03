// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_SEPARATOR_H_
#define VIEWS_CONTROLS_SEPARATOR_H_
#pragma once

#include <string>

#include "views/view.h"

namespace views {

// The Separator class is a view that shows a line used to visually separate
// other views.  The current implementation is only horizontal.

class Separator : public View {
 public:
  // The separator's class name.
  static const char kViewClassName[];

  Separator();
  virtual ~Separator();

  // Overridden from View:
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

 protected:
  // Overridden from View:
  virtual void ViewHierarchyChanged(bool is_add, View* parent,
                                    View* child) OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

 private:
  void CreateNativeWrapper();

  // The native view.
  View* native_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(Separator);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_SEPARATOR_H_
