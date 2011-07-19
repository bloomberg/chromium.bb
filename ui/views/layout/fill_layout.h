// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_FILL_LAYOUT_H_
#define UI_VIEWS_LAYOUT_FILL_LAYOUT_H_
#pragma once

#include "base/logging.h"
#include "ui/views/layout/layout_manager.h"

namespace ui {

////////////////////////////////////////////////////////////////////////////////
// FillLayout class
//
//  A simple LayoutManager that compels a single view to fit its parent.
//
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

}  // namespace ui

#endif  // UI_VIEWS_LAYOUT_FILL_LAYOUT_H_

