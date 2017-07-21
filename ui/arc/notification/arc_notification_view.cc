// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_notification_view.h"

#include <algorithm>

#include "ui/arc/notification/arc_notification_content_view.h"
#include "ui/arc/notification/arc_notification_content_view_delegate.h"
#include "ui/arc/notification/arc_notification_item.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/painter.h"

namespace arc {

// static
const char ArcNotificationView::kViewClassName[] = "ArcNotificationView";

ArcNotificationView::ArcNotificationView(
    std::unique_ptr<views::View> contents_view,
    std::unique_ptr<ArcNotificationContentViewDelegate> contents_view_delegate,
    message_center::MessageCenterController* controller,
    const message_center::Notification& notification)
    : message_center::MessageView(controller, notification),
      contents_view_(contents_view.get()),
      contents_view_delegate_(std::move(contents_view_delegate)) {
  DCHECK_EQ(message_center::NOTIFICATION_TYPE_CUSTOM, notification.type());

  DCHECK(contents_view);
  AddChildView(contents_view.release());

  DCHECK(contents_view_delegate_);

  if (contents_view_->background()) {
    background_view()->background()->SetNativeControlColor(
        contents_view_->background()->get_color());
  }

  // Creates the control_buttons_view_, which collects all control buttons into
  // a horizontal box.
  DCHECK(!control_buttons_view_);
  control_buttons_view_ =
      new message_center::NotificationControlButtonsView(this);
  AddChildView(control_buttons_view_);
  control_buttons_view_->ReparentToWidgetLayer();

  control_buttons_view_->ShowCloseButton(!notification.pinned());
  control_buttons_view_->ShowSettingsButton(
      notification.delegate()->ShouldDisplaySettingsButton());

  focus_painter_ = views::Painter::CreateSolidFocusPainter(
      message_center::kFocusBorderColor, gfx::Insets(0, 1, 3, 2));
}

ArcNotificationView::~ArcNotificationView() = default;

void ArcNotificationView::OnContentFocused() {
  UpdateControlButtonsVisibility();
  SchedulePaint();
}

void ArcNotificationView::OnContentBlured() {
  UpdateControlButtonsVisibility();
  SchedulePaint();
}

void ArcNotificationView::SetDrawBackgroundAsActive(bool active) {
  // Do nothing if |contents_view_| has a background.
  if (contents_view_->background())
    return;

  message_center::MessageView::SetDrawBackgroundAsActive(active);
}

void ArcNotificationView::SetControlButtonsBackgroundColor(
    const SkColor& bgcolor) {
  control_buttons_view_->SetBackgroundColor(bgcolor);
}

void ArcNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  message_center::MessageView::UpdateWithNotification(notification);

  control_buttons_view_->ShowCloseButton(!notification.pinned());
  control_buttons_view_->ShowSettingsButton(
      notification.delegate()->ShouldDisplaySettingsButton());
}

bool ArcNotificationView::IsCloseButtonFocused() const {
  return control_buttons_view_->IsCloseButtonFocused();
}

void ArcNotificationView::RequestFocusOnCloseButton() {
  control_buttons_view_->RequestFocusOnCloseButton();
  UpdateControlButtonsVisibility();
}

const char* ArcNotificationView::GetClassName() const {
  return kViewClassName;
}

void ArcNotificationView::UpdateControlButtonsVisibility() {
  if (!control_buttons_view_)
    return;

  const bool target_visibility =
      IsMouseHovered() || (control_buttons_view_->IsCloseButtonFocused()) ||
      (control_buttons_view_->IsSettingsButtonFocused());

  if (target_visibility == control_buttons_view_->GetButtonsVisible())
    return;

  control_buttons_view_->SetButtonsVisible(target_visibility);
}

void ArcNotificationView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // This data is never used since this view is never focused when the content
  // view is focusable.
}

void ArcNotificationView::OnSlideChanged() {
  if (contents_view_delegate_)
    contents_view_delegate_->OnSlideChanged();
}

gfx::Size ArcNotificationView::CalculatePreferredSize() const {
  const gfx::Insets insets = GetInsets();
  const int contents_width =
      message_center::kNotificationWidth - insets.width();
  const int contents_height = contents_view_->GetHeightForWidth(contents_width);
  return gfx::Size(message_center::kNotificationWidth,
                   contents_height + insets.height());
}

void ArcNotificationView::Layout() {
  message_center::MessageView::Layout();

  const gfx::Rect contents_bounds = GetContentsBounds();
  contents_view_->SetBoundsRect(contents_bounds);

  if (control_buttons_view_) {
    gfx::Rect control_buttons_bounds(contents_bounds);
    int buttons_width = control_buttons_view_->GetPreferredSize().width();
    int buttons_height = control_buttons_view_->GetPreferredSize().height();

    control_buttons_bounds.set_x(control_buttons_bounds.right() -
                                 buttons_width -
                                 message_center::kControlButtonPadding);
    control_buttons_bounds.set_y(control_buttons_bounds.y() +
                                 message_center::kControlButtonPadding);
    control_buttons_bounds.set_width(buttons_width);
    control_buttons_bounds.set_height(buttons_height);
    control_buttons_view_->SetBoundsRect(control_buttons_bounds);
  }

  UpdateControlButtonsVisibility();

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

bool ArcNotificationView::HitTestControlButtons(const gfx::Point& point) {
  gfx::Point point_in_buttons = point;
  views::View::ConvertPointToTarget(this, control_buttons_view_,
                                    &point_in_buttons);
  return control_buttons_view_->HitTestPoint(point_in_buttons);
}

void ArcNotificationView::OnMouseEntered(const ui::MouseEvent&) {
  UpdateControlButtonsVisibility();
}

void ArcNotificationView::OnMouseExited(const ui::MouseEvent&) {
  UpdateControlButtonsVisibility();
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

void ArcNotificationView::OnLayerBoundsChanged(const gfx::Rect& old_bounds) {
  control_buttons_view_->AdjustLayerBounds();
}

void ArcNotificationView::ChildPreferredSizeChanged(View* child) {
  // Notify MessageCenterController when the custom content changes size,
  // as it may need to relayout.
  if (controller())
    controller()->UpdateNotificationSize(notification_id());
}

bool ArcNotificationView::HandleAccessibleAction(
    const ui::AXActionData& action) {
  if (contents_view_)
    return contents_view_->HandleAccessibleAction(action);
  return false;
}

}  // namespace message_center
