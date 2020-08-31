// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_IME_MODE_INDICATOR_VIEW_H_
#define ASH_IME_IME_MODE_INDICATOR_VIEW_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

// A small bubble that shows the short name of the current IME (e.g. "DV" for
// Dvorak) after switching IMEs with an accelerator (e.g. Ctrl-Space).
class ASH_EXPORT ImeModeIndicatorView : public views::BubbleDialogDelegateView {
 public:
  // The cursor bounds is in the universal screen coordinates in DIP.
  ImeModeIndicatorView(const gfx::Rect& cursor_bounds,
                       const base::string16& label);
  ~ImeModeIndicatorView() override;

  // Show the mode indicator then hide with fading animation.
  void ShowAndFadeOut();

  // views::BubbleDialogDelegateView override:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;
  void Init() override;

 protected:
  // views::WidgetDelegateView overrides:
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;

 private:
  gfx::Rect cursor_bounds_;
  views::Label* label_view_;
  base::OneShotTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(ImeModeIndicatorView);
};

}  // namespace ash

#endif  // ASH_IME_IME_MODE_INDICATOR_VIEW_H_
