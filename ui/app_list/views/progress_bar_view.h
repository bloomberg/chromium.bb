// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_PROGRESS_BAR_VIEW_H_
#define UI_APP_LIST_VIEWS_PROGRESS_BAR_VIEW_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/progress_bar.h"

namespace views {
class Painter;
}

namespace app_list {

// ProgressBarView implements an image-based progress bar for app launcher.
class ProgressBarView : public views::ProgressBar {
 public:
  ProgressBarView();
  virtual ~ProgressBarView();

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  scoped_ptr<views::Painter> background_painter_;
  scoped_ptr<views::Painter> bar_painter_;

  DISALLOW_COPY_AND_ASSIGN(ProgressBarView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_PROGRESS_BAR_VIEW_H_
