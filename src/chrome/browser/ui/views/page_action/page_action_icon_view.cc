// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

#include <utility>

#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/style/platform_style.h"

bool PageActionIconView::Delegate::IsLocationBarUserInputInProgress() const {
  return false;
}

const OmniboxView* PageActionIconView::Delegate::GetOmniboxView() const {
  // Should not reach here: should call subclass's implementation.
  NOTREACHED();
  return nullptr;
}

PageActionIconView::PageActionIconView(CommandUpdater* command_updater,
                                       int command_id,
                                       PageActionIconView::Delegate* delegate,
                                       const gfx::FontList& font_list)
    : IconLabelBubbleView(font_list),
      command_updater_(command_updater),
      delegate_(delegate),
      command_id_(command_id) {
  DCHECK(delegate_);

  image()->EnableCanvasFlippingForRTLUI(true);
  SetInkDropMode(InkDropMode::ON);
  set_ink_drop_visible_opacity(
      GetOmniboxStateOpacity(OmniboxPartState::SELECTED));
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  // Only shows bubble after mouse is released.
  set_notify_action(NotifyAction::NOTIFY_ON_RELEASE);
}

PageActionIconView::~PageActionIconView() {}

bool PageActionIconView::IsBubbleShowing() const {
  // If the bubble is being destroyed, it's considered showing though it may be
  // already invisible currently.
  return GetBubble() != nullptr;
}

bool PageActionIconView::SetCommandEnabled(bool enabled) const {
  DCHECK(command_updater_);
  command_updater_->UpdateCommandEnabled(command_id_, enabled);
  return command_updater_->IsCommandEnabled(command_id_);
}

bool PageActionIconView::Update() {
  return false;
}

SkColor PageActionIconView::GetLabelColorForTesting() const {
  return label()->enabled_color();
}

void PageActionIconView::ExecuteForTesting() {
  DCHECK(GetVisible());
  OnExecuting(EXECUTE_SOURCE_MOUSE);
}

SkColor PageActionIconView::GetTextColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
}

void PageActionIconView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
  node_data->SetName(GetTextForTooltipAndAccessibleName());
}

base::string16 PageActionIconView::GetTooltipText(const gfx::Point& p) const {
  return IsBubbleShowing() ? base::string16()
                           : GetTextForTooltipAndAccessibleName();
}

void PageActionIconView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  View::ViewHierarchyChanged(details);
  if (details.is_add && details.child == this && GetNativeTheme())
    UpdateIconImage();
}

void PageActionIconView::OnThemeChanged() {
  IconLabelBubbleView::OnThemeChanged();
  UpdateIconImage();
}

SkColor PageActionIconView::GetInkDropBaseColor() const {
  return delegate_->GetPageActionInkDropColor();
}

bool PageActionIconView::ShouldShowSeparator() const {
  return false;
}

void PageActionIconView::NotifyClick(const ui::Event& event) {
  // Intentionally skip the immediate parent function
  // IconLabelBubbleView::NotifyClick(). It calls ShowBubble() which
  // is redundant here since we use Chrome command to show the bubble.
  LabelButton::NotifyClick(event);
  ExecuteSource source;
  if (event.IsMouseEvent()) {
    source = EXECUTE_SOURCE_MOUSE;
  } else if (event.IsKeyEvent()) {
    source = EXECUTE_SOURCE_KEYBOARD;
  } else if (event.IsGestureEvent()) {
    source = EXECUTE_SOURCE_GESTURE;
  } else {
    NOTREACHED();
    return;
  }

  // Set ink drop state to ACTIVATED.
  SetHighlighted(true);
  ExecuteCommand(source);
}

bool PageActionIconView::IsTriggerableEvent(const ui::Event& event) {
  // For PageActionIconView, returns whether the bubble should be shown given
  // the event happened. For mouse event, only shows bubble when the bubble is
  // not visible and when event is a left button click.
  if (event.IsMouseEvent()) {
    // IconLabelBubbleView allows any mouse click to be triggerable event so
    // need to manually check here.
    return IconLabelBubbleView::IsTriggerableEvent(event) &&
           ((triggerable_event_flags() & event.flags()) != 0);
  }

  return IconLabelBubbleView::IsTriggerableEvent(event);
}

bool PageActionIconView::ShouldUpdateInkDropOnClickCanceled() const {
  // Override IconLabelBubbleView since for PageActionIconView if click is
  // cancelled due to bubble being visible, the InkDropState is ACTIVATED. So
  // the ink drop will not be updated anyway. Setting this to true will help to
  // update ink drop in other cases where clicks are cancelled.
  return true;
}

void PageActionIconView::ExecuteCommand(ExecuteSource source) {
  OnExecuting(source);
  if (command_updater_)
    command_updater_->ExecuteCommand(command_id_);
}

const gfx::VectorIcon& PageActionIconView::GetVectorIconBadge() const {
  return gfx::kNoneIcon;
}

void PageActionIconView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  views::BubbleDialogDelegateView* bubble = GetBubble();
  if (bubble)
    bubble->OnAnchorBoundsChanged();
  IconLabelBubbleView::OnBoundsChanged(previous_bounds);
}

void PageActionIconView::OnTouchUiChanged() {
  icon_size_ = GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
  UpdateIconImage();
  IconLabelBubbleView::OnTouchUiChanged();
}

void PageActionIconView::UpdateBorder() {
  SetBorder(views::CreateEmptyBorder(
      GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING)));
}

void PageActionIconView::SetIconColor(SkColor icon_color) {
  icon_color_ = icon_color;
  UpdateIconImage();
}

void PageActionIconView::UpdateIconImage() {
  const ui::NativeTheme* theme = GetNativeTheme();
  SkColor icon_color = active_
                           ? theme->GetSystemColor(
                                 ui::NativeTheme::kColorId_ProminentButtonColor)
                           : icon_color_;
  SetImage(gfx::CreateVectorIconWithBadge(GetVectorIcon(), icon_size_,
                                          icon_color, GetVectorIconBadge()));
}

void PageActionIconView::SetActiveInternal(bool active) {
  if (active_ == active)
    return;
  active_ = active;
  UpdateIconImage();
}

content::WebContents* PageActionIconView::GetWebContents() const {
  return delegate_->GetWebContentsForPageActionIconView();
}
