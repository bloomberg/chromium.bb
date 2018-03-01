// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_notification_view.h"

#include <algorithm>

#include "ui/accessibility/ax_action_data.h"
#include "ui/arc/notification/arc_notification_content_view_delegate.h"
#include "ui/arc/notification/arc_notification_item.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/painter.h"

namespace arc {

// static
const char ArcNotificationView::kViewClassName[] = "ArcNotificationView";

ArcNotificationView::ArcNotificationView(
    ArcNotificationItem* item,
    std::unique_ptr<views::View> contents_view,
    std::unique_ptr<ArcNotificationContentViewDelegate> contents_view_delegate,
    const message_center::Notification& notification)
    : message_center::MessageView(notification),
      item_(item),
      contents_view_(contents_view.get()),
      contents_view_delegate_(std::move(contents_view_delegate)) {
  DCHECK_EQ(message_center::NOTIFICATION_TYPE_CUSTOM, notification.type());

  item_->AddObserver(this);

  DCHECK(contents_view);
  AddChildView(contents_view.release());

  DCHECK(contents_view_delegate_);

  if (contents_view_->background()) {
    background_view()->background()->SetNativeControlColor(
        contents_view_->background()->get_color());
  }

  UpdateControlButtonsVisibilityWithNotification(notification);

  focus_painter_ = views::Painter::CreateSolidFocusPainter(
      message_center::kFocusBorderColor, gfx::Insets(0, 1, 3, 2));
}

ArcNotificationView::~ArcNotificationView() {
  if (item_)
    item_->RemoveObserver(this);
}

void ArcNotificationView::OnContentFocused() {
  SchedulePaint();
}

void ArcNotificationView::OnContentBlured() {
  SchedulePaint();
}

void ArcNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  message_center::MessageView::UpdateWithNotification(notification);

  UpdateControlButtonsVisibilityWithNotification(notification);
}

void ArcNotificationView::SetDrawBackgroundAsActive(bool active) {
  // Do nothing if |contents_view_| has a background.
  if (contents_view_->background())
    return;

  message_center::MessageView::SetDrawBackgroundAsActive(active);
}

bool ArcNotificationView::IsCloseButtonFocused() const {
  if (!GetControlButtonsView())
    return false;
  return GetControlButtonsView()->IsCloseButtonFocused();
}

void ArcNotificationView::RequestFocusOnCloseButton() {
  if (GetControlButtonsView()) {
    GetControlButtonsView()->RequestFocusOnCloseButton();
    if (contents_view_delegate_)
      contents_view_delegate_->UpdateControlButtonsVisibility();
  }
}

const char* ArcNotificationView::GetClassName() const {
  return kViewClassName;
}

void ArcNotificationView::UpdateControlButtonsVisibility() {
  if (contents_view_delegate_)
    contents_view_delegate_->UpdateControlButtonsVisibility();
}

void ArcNotificationView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // This data is never used since this view is never focused when the content
  // view is focusable.
}

message_center::NotificationControlButtonsView*
ArcNotificationView::GetControlButtonsView() const {
  return contents_view_delegate_->GetControlButtonsView();
}

bool ArcNotificationView::IsExpanded() const {
  return item_ &&
         item_->GetExpandState() == mojom::ArcNotificationExpandState::EXPANDED;
}

void ArcNotificationView::SetExpanded(bool expanded) {
  if (!item_)
    return;

  auto expand_state = item_->GetExpandState();
  if (expanded) {
    if (expand_state == mojom::ArcNotificationExpandState::COLLAPSED)
      item_->ToggleExpansion();
  } else {
    if (expand_state == mojom::ArcNotificationExpandState::EXPANDED)
      item_->ToggleExpansion();
  }
}

bool ArcNotificationView::IsManuallyExpandedOrCollapsed() const {
  if (item_)
    return item_->IsManuallyExpandedOrCollapsed();
  return false;
}

void ArcNotificationView::OnContainerAnimationStarted() {
  if (contents_view_delegate_)
    contents_view_delegate_->OnContainerAnimationStarted();
}

void ArcNotificationView::OnContainerAnimationEnded() {
  if (contents_view_delegate_)
    contents_view_delegate_->OnContainerAnimationEnded();
}

void ArcNotificationView::OnSlideChanged() {
  if (contents_view_delegate_)
    contents_view_delegate_->OnSlideChanged();
}

gfx::Size ArcNotificationView::CalculatePreferredSize() const {
  const gfx::Insets insets = GetInsets();
  const int contents_width = message_center::kNotificationWidth;
  const int contents_height = contents_view_->GetHeightForWidth(contents_width);
  return gfx::Size(contents_width + insets.width(),
                   contents_height + insets.height());
}

void ArcNotificationView::Layout() {
  // Setting the bounds before calling the parent to prevent double Layout.
  contents_view_->SetBoundsRect(GetContentsBounds());

  message_center::MessageView::Layout();

  // If the content view claims focus, defer focus handling to the content view.
  if (contents_view_->IsFocusable())
    SetFocusBehavior(FocusBehavior::NEVER);
}

bool ArcNotificationView::HasFocus() const {
  // In case that focus handling is defered to the content view, asking the
  // content view about focus.
  if (contents_view_ && contents_view_->IsFocusable())
    return contents_view_->HasFocus();
  else
    return message_center::MessageView::HasFocus();
}

void ArcNotificationView::RequestFocus() {
  if (contents_view_ && contents_view_->IsFocusable())
    contents_view_->RequestFocus();
  else
    message_center::MessageView::RequestFocus();
}

void ArcNotificationView::OnPaint(gfx::Canvas* canvas) {
  MessageView::OnPaint(canvas);
  if (contents_view_ && contents_view_->IsFocusable())
    views::Painter::PaintFocusPainter(contents_view_, canvas,
                                      focus_painter_.get());
}

bool ArcNotificationView::OnKeyPressed(const ui::KeyEvent& event) {
  if (contents_view_) {
    ui::InputMethod* input_method = contents_view_->GetInputMethod();
    if (input_method) {
      ui::TextInputClient* text_input_client =
          input_method->GetTextInputClient();
      if (text_input_client &&
          text_input_client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE) {
        // If the focus is in an edit box, we skip the special key handling for
        // back space and return keys. So that these key events are sent to the
        // arc container correctly without being handled by the message center.
        return false;
      }
    }
  }

  return message_center::MessageView::OnKeyPressed(event);
}

void ArcNotificationView::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
}

bool ArcNotificationView::HandleAccessibleAction(
    const ui::AXActionData& action) {
  if (item_ && action.action == ax::mojom::Action::kDoDefault) {
    item_->ToggleExpansion();
    return true;
  }
  return false;
}

void ArcNotificationView::OnItemDestroying() {
  DCHECK(item_);
  item_->RemoveObserver(this);
  item_ = nullptr;
}

void ArcNotificationView::OnItemUpdated() {}

// TODO(yoshiki): move this to MessageView and share the code among
// NotificationView and NotificationViewMD.
void ArcNotificationView::UpdateControlButtonsVisibilityWithNotification(
    const message_center::Notification& notification) {
  if (!GetControlButtonsView())
    return;

  GetControlButtonsView()->ShowSettingsButton(
      notification.should_show_settings_button());
  GetControlButtonsView()->ShowCloseButton(!GetPinned());
  UpdateControlButtonsVisibility();
}

}  // namespace message_center
