// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/dictation_bubble_controller.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_bubble_view.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/views/widget/widget.h"

namespace ash {

DictationBubbleController::DictationBubbleController() {
  ui::InputMethod* input_method =
      Shell::Get()->window_tree_host_manager()->input_method();
  if (!input_method_observer_.IsObservingSource(input_method))
    input_method_observer_.Observe(input_method);
}

DictationBubbleController::~DictationBubbleController() {
  if (widget_ && !widget_->IsClosed())
    widget_->CloseNow();
}

void DictationBubbleController::UpdateBubble(
    bool visible,
    DictationBubbleIconType icon,
    const absl::optional<std::u16string>& text) {
  if (visible) {
    MaybeInitialize();
    Update(icon, text);
    widget_->Show();
  } else {
    Update(icon, text);
    if (widget_) {
      widget_->Hide();
    }
  }
}

void DictationBubbleController::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
  if (!client || client->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE ||
      !dictation_bubble_view_ || !dictation_bubble_view_->GetVisible()) {
    return;
  }

  const gfx::Rect new_caret_bounds = client->GetCaretBounds();
  if (new_caret_bounds == dictation_bubble_view_->GetAnchorRect())
    return;

  // Update the position of `dictation_bubble_view_` to match the current caret
  // location.
  dictation_bubble_view_->SetAnchorRect(new_caret_bounds);
}

void DictationBubbleController::MaybeInitialize() {
  if (widget_)
    return;

  dictation_bubble_view_ = new DictationBubbleView();
  widget_ =
      views::BubbleDialogDelegateView::CreateBubble(dictation_bubble_view_);
  widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingUIElement);
  CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
      widget_->GetNativeWindow(),
      CollisionDetectionUtils::RelativePriority::kDictationBubble);
}

// TODO(crbug.com/1252037): Fix issue where bubble is shown behind current
// Chrome tab.
void DictationBubbleController::Update(
    DictationBubbleIconType icon,
    const absl::optional<std::u16string>& text) {
  DCHECK(dictation_bubble_view_);
  DCHECK(widget_);

  // Update `dictation_bubble_view_`.
  dictation_bubble_view_->Update(icon, text);

  // Update the bounds to fit entirely within the screen.
  gfx::Rect new_bounds = widget_->GetWindowBoundsInScreen();
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetDisplayMatching(new_bounds).bounds();
  new_bounds.AdjustToFit(display_bounds);

  // Update the preferred bounds based on other system windows.
  gfx::Rect resting_bounds = CollisionDetectionUtils::AvoidObstacles(
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget_->GetNativeWindow()),
      new_bounds, CollisionDetectionUtils::RelativePriority::kDictationBubble);
  widget_->SetBounds(resting_bounds);
}

}  // namespace ash
