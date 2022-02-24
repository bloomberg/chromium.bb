// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_HELP_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_HELP_BUBBLE_VIEW_H_

#include <cstddef>
#include <memory>

#include "base/timer/timer.h"
#include "chrome/browser/ui/user_education/help_bubble_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace ui {
class MouseEvent;
}  // namespace ui

namespace views {
class MdTextButton;
}  // namespace views

// The HelpBubbleView is a special BubbleDialogDelegateView for
// in-product help which educates users about certain Chrome features in
// a deferred context.
class HelpBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(HelpBubbleView);
  HelpBubbleView(views::View* anchor_view,
                 HelpBubbleParams params,
                 absl::optional<gfx::Rect> anchor_rect = absl::nullopt);
  HelpBubbleView(const HelpBubbleView&) = delete;
  HelpBubbleView& operator=(const HelpBubbleView&) = delete;
  ~HelpBubbleView() override;

  // Returns whether the given dialog is a help bubble.
  static bool IsHelpBubble(views::DialogDelegate* dialog);

  views::Button* GetButtonForTesting(int index) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(HelpBubbleViewTest,
                           RespectsProvidedTimeoutAfterActivate);

  void MaybeStartAutoCloseTimer();

  void OnTimeout();

  // BubbleDialogDelegateView:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  std::u16string GetAccessibleWindowTitle() const override;
  void UpdateHighlightedButton(bool highlighted) override {
    // Do nothing: the anchor for promo bubbles should not highlight.
  }
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  gfx::Size CalculatePreferredSize() const override;
  gfx::Rect GetAnchorRect() const override;

  // Forces the anchor rect to the specified rectangle (in screen coordinates).
  // If an artificial anchor rect is used, we assume the exact target cannot be
  // localized, and a visible arrow is not shown.
  absl::optional<gfx::Rect> force_anchor_rect_;

  // If the bubble has buttons, it must be focusable.
  std::vector<views::MdTextButton*> buttons_;

  // This is the base accessible name of the window.
  std::u16string accessible_name_;

  // This is any additional hint text to read.
  std::u16string screenreader_hint_text_;

  // Track the number of times the widget has been activated; if it's greater
  // than 1 we won't re-read the screenreader hint again.
  int activate_count_ = 0;

  // Auto close timeout. If the value is 0 (default), the bubble never times
  // out.
  base::TimeDelta timeout_;
  base::OneShotTimer auto_close_timer_;

  base::OnceClosure timeout_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_HELP_BUBBLE_VIEW_H_
