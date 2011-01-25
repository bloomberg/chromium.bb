// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_LAYOUT_FILL_LAYOUT_H_
#define VIEWS_LAYOUT_FILL_LAYOUT_H_
#pragma once

#include "views/layout_manager.h"
#include "views/view.h"

namespace views {

///////////////////////////////////////////////////////////////////////////////
//
// FillLayout
//  A simple LayoutManager that causes the associated view's one child to be
//  sized to match the bounds of its parent.
//
///////////////////////////////////////////////////////////////////////////////
class FillLayout : public LayoutManager {
 public:
  FillLayout();
  virtual ~FillLayout();

  // Overridden from LayoutManager:
  virtual void Layout(View* host);
  virtual gfx::Size GetPreferredSize(View* host);

 private:
  DISALLOW_COPY_AND_ASSIGN(FillLayout);
};

}  // namespace views

#endif  // VIEWS_LAYOUT_FILL_LAYOUT_H_
