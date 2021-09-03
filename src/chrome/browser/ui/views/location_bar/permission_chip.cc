// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/permission_chip.h"

#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_style.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/widget/widget.h"

// ButtonController that NotifyClick from being called when the
// BubbleOwnerDelegate's bubble is showing. Otherwise the bubble will show again
// immediately after being closed via losing focus.
class BubbleButtonController : public views::ButtonController {
 public:
  BubbleButtonController(
      views::Button* button,
      BubbleOwnerDelegate* bubble_owner,
      std::unique_ptr<views::ButtonControllerDelegate> delegate)
      : views::ButtonController(button, std::move(delegate)),
        bubble_owner_(bubble_owner) {}

  bool OnMousePressed(const ui::MouseEvent& event) override {
    suppress_button_release_ = bubble_owner_->IsBubbleShowing();
    return views::ButtonController::OnMousePressed(event);
  }

  bool IsTriggerableEvent(const ui::Event& event) override {
    // TODO(olesiamarukhno): There is the same logic in IconLabelBubbleView,
    // this class should be reused in the future to avoid duplication.
    if (event.IsMouseEvent())
      return !bubble_owner_->IsBubbleShowing() && !suppress_button_release_;

    return views::ButtonController::IsTriggerableEvent(event);
  }

 private:
  bool suppress_button_release_ = false;
  BubbleOwnerDelegate* bubble_owner_ = nullptr;
};

PermissionChip::PermissionChip(
    permissions::PermissionPrompt::Delegate* delegate,
    const gfx::VectorIcon& icon,
    std::u16string message,
    bool should_start_open)
    : delegate_(delegate), should_start_open_(should_start_open) {
  DCHECK(delegate);
  SetUseDefaultFillLayout(true);

  chip_button_ = AddChildView(std::make_unique<OmniboxChipButton>(
      base::BindRepeating(&PermissionChip::ChipButtonPressed,
                          base::Unretained(this)),
      icon, message, true));

  chip_button_->SetButtonController(std::make_unique<BubbleButtonController>(
      chip_button_, this,
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(
          chip_button_)));

  chip_button_->SetExpandAnimationEndedCallback(base::BindRepeating(
      &PermissionChip::ExpandAnimationEnded, base::Unretained(this)));

  chip_button_->SetTheme(OmniboxChipButton::Theme::kBlue);

  Show(should_start_open_);
}

PermissionChip::~PermissionChip() {
  CHECK(!IsInObserverList());
  collapse_timer_.AbandonAndStop();
  dismiss_timer_.AbandonAndStop();
}

void PermissionChip::Hide() {
  SetVisible(false);
}

void PermissionChip::Reshow() {
  if (GetVisible())
    return;
  SetVisible(true);
  Show(/*always_open_bubble=*/false);
}

void PermissionChip::OnMouseEntered(const ui::MouseEvent& event) {
  if (!chip_button_->is_animating())
    RestartTimersOnInteraction();
}

void PermissionChip::AddedToWidget() {
  views::AccessiblePaneView::AddedToWidget();

  if (!should_start_open_) {
    GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
        IDS_PERMISSIONS_REQUESTED_SCREENREADER_ANNOUNCEMENT));
  }
}

void PermissionChip::OnWidgetClosing(views::Widget* widget) {
  widget->RemoveObserver(this);
  // If permission request is still active after the prompt was closed,
  // collapse the chip.
  Collapse(/*allow_restart=*/false);
}

bool PermissionChip::IsBubbleShowing() const {
  return false;
}

void PermissionChip::Show(bool always_open_bubble) {
  // TODO(olesiamarukhno): Add tests for animation logic.
  chip_button_->ResetAnimation();
  if (!delegate_->WasCurrentRequestAlreadyDisplayed() || always_open_bubble) {
    chip_button_->AnimateExpand();
  } else {
    StartDismissTimer();
  }
  PreferredSizeChanged();
}

void PermissionChip::ExpandAnimationEnded() {
  StartCollapseTimer();
  if (should_start_open_ && !IsBubbleShowing())
    OpenBubble();
}

void PermissionChip::ChipButtonPressed() {
  OpenBubble();
  RestartTimersOnInteraction();
}

void PermissionChip::RestartTimersOnInteraction() {
  if (is_fully_collapsed()) {
    StartDismissTimer();
  } else {
    StartCollapseTimer();
  }
}

void PermissionChip::StartCollapseTimer() {
  constexpr auto kDelayBeforeCollapsingChip = base::TimeDelta::FromSeconds(12);
  collapse_timer_.Start(
      FROM_HERE, kDelayBeforeCollapsingChip,
      base::BindOnce(&PermissionChip::Collapse, base::Unretained(this),
                     /*allow_restart=*/true));
}

void PermissionChip::Collapse(bool allow_restart) {
  if (allow_restart && (IsMouseHovered() || IsBubbleShowing())) {
    StartCollapseTimer();
  } else {
    chip_button_->AnimateCollapse();
    StartDismissTimer();
  }
}

void PermissionChip::StartDismissTimer() {
  constexpr auto kDelayBeforeDismissingRequest =
      base::TimeDelta::FromSeconds(6);
  dismiss_timer_.Start(FROM_HERE, kDelayBeforeDismissingRequest, this,
                       &PermissionChip::Dismiss);
}

void PermissionChip::Dismiss() {
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      IDS_PERMISSIONS_EXPIRED_SCREENREADER_ANNOUNCEMENT));

  // `delegate_->Closing()` will destroy `this`. It's not safe to run any code
  // afterwards.
  delegate_->Closing();
}

BEGIN_METADATA(PermissionChip, views::View)
END_METADATA
