// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_FOCUS_RING_H_
#define UI_VIEWS_CONTROLS_FOCUS_RING_H_

#include "ui/views/view.h"

namespace views {

// FocusRing is a View that is designed to act as an indicator of focus for its
// parent. It is a stand-alone view that paints to a layer which extends beyond
// the bounds of its parent view.
class FocusRing : public View {
 public:
  // Create a FocusRing and adds it to |parent|.
  static void Install(views::View* parent);

  // Removes the FocusRing from |parent|.
  static void Uninstall(views::View* parent);

  // View:
  const char* GetClassName() const override;
  bool CanProcessEventsWithinSubtree() const override;
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  static const char kViewClassName[];

  FocusRing();
  ~FocusRing() override;

  DISALLOW_COPY_AND_ASSIGN(FocusRing);
};

}  // views

#endif  // UI_VIEWS_CONTROLS_FOCUS_RING_H_
