// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_notification_view.h"

#include <algorithm>

#include "ui/arc/notification/arc_notification_content_view_delegate.h"
#include "ui/arc/notification/arc_notification_item.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/message_center_controller.h"
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

  focus_painter_ = views::Painter::CreateSolidFocusPainter(
      message_center::kFocusBorderColor, gfx::Insets(0, 1, 3, 2));
}

ArcNotificationView::~ArcNotificationView() = default;

void ArcNotificationView::OnContentFocused() {
  SchedulePaint();
}

void ArcNotificationView::OnContentBlured() {
  SchedulePaint();
}

void ArcNotificationView::SetDrawBackgroundAsActive(bool active) {
  // Do nothing if |contents_view_| has a background.
  if (contents_view_->background())
    return;

  message_center::MessageView::SetDrawBackgroundAsActive(active);
}

bool ArcNotificationView::IsCloseButtonFocused() const {
  if (!contents_view_delegate_)
    return false;
  return contents_view_delegate_->IsCloseButtonFocused();
}

void ArcNotificationView::RequestFocusOnCloseButton() {
  if (contents_view_delegate_)
    contents_view_delegate_->RequestFocusOnCloseButton();
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

  contents_view_->SetBoundsRect(GetContentsBounds());

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
