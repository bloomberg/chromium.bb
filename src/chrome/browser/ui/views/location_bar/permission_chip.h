// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_CHIP_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_CHIP_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
class BubbleDialogDelegateView;
}  // namespace views

class BubbleOwnerDelegate {
 public:
  virtual bool IsBubbleShowing() const = 0;
};

// A class for an interface for chip view that is shown in the location bar to
// notify user about a permission request.
class PermissionChip : public views::AccessiblePaneView,
                       public views::WidgetObserver,
                       public BubbleOwnerDelegate {
 public:
  METADATA_HEADER(PermissionChip);
  PermissionChip(permissions::PermissionPrompt::Delegate* delegate,
                 const gfx::VectorIcon& icon,
                 std::u16string message,
                 bool should_start_open);
  PermissionChip(const PermissionChip& chip) = delete;
  PermissionChip& operator=(const PermissionChip& chip) = delete;
  ~PermissionChip() override;

  // Opens the permission prompt bubble.
  virtual void OpenBubble() = 0;

  void Hide();
  void Reshow();

  views::Button* button() { return chip_button_; }
  bool is_fully_collapsed() const { return chip_button_->is_fully_collapsed(); }

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void AddedToWidget() override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

  // BubbleOwnerDelegate:
  bool IsBubbleShowing() const override;

  virtual views::BubbleDialogDelegateView*
  GetPermissionPromptBubbleForTest() = 0;

  bool should_start_open_for_testing() { return should_start_open_; }

 protected:
  permissions::PermissionPrompt::Delegate* delegate() const {
    return delegate_;
  }

 private:
  void Show(bool always_open_bubble);
  void ExpandAnimationEnded();
  void ChipButtonPressed();
  void RestartTimersOnInteraction();
  void StartCollapseTimer();
  void Collapse(bool allow_restart);
  void StartDismissTimer();
  void Dismiss();
  void AnimateCollapse();
  void AnimateExpand();

  permissions::PermissionPrompt::Delegate* const delegate_;

  // A timer used to collapse the chip after a delay.
  base::OneShotTimer collapse_timer_;

  // A timer used to dismiss the permission request after it's been collapsed
  // for a while.
  base::OneShotTimer dismiss_timer_;

  // The button that displays the icon and text.
  OmniboxChipButton* chip_button_ = nullptr;

  bool should_start_open_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_CHIP_H_
