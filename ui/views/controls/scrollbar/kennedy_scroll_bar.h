// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_KENNEDY_SCROLL_BAR_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_KENNEDY_SCROLL_BAR_H_

#include "base/macros.h"
#include "ui/views/controls/scrollbar/base_scroll_bar.h"

namespace views {

// The scrollbar of kennedy style. Transparent track and grey rectangle
// thumb. Right now it doesn't have the way to share the background,
// so it will accept the background color instead.
class VIEWS_EXPORT KennedyScrollBar : public BaseScrollBar {
 public:
  explicit KennedyScrollBar(bool horizontal);
  ~KennedyScrollBar() override;

 protected:
  // BaseScrollBar overrides:
  gfx::Rect GetTrackBounds() const override;

  // ScrollBar overrides:
  int GetLayoutSize() const override;

  // View overrides:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(KennedyScrollBar);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_KENNEDY_SCROLL_BAR_H_
