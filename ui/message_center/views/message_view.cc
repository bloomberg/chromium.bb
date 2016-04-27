// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_view.h"

#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/shadow_value.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/painter.h"
#include "ui/views/shadow_border.h"

namespace {

const int kShadowOffset = 1;
const int kShadowBlur = 4;

}  // namespace

namespace message_center {

MessageView::MessageView(MessageViewController* controller,
                         const std::string& notification_id,
                         const NotifierId& notifier_id,
                         const gfx::ImageSkia& small_image,
                         const base::string16& display_source)
    : controller_(controller),
      notification_id_(notification_id),
      notifier_id_(notifier_id),
      background_view_(NULL),
      scroller_(NULL),
      display_source_(display_source) {
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // Create the opaque background that's above the view's shadow.
  background_view_ = new views::View();
  background_view_->set_background(
      views::Background::CreateSolidBackground(kNotificationBackgroundColor));
  AddChildView(background_view_);

  views::ImageView* small_image_view = new views::ImageView();
  small_image_view->SetImage(small_image);
  small_image_view->SetImageSize(gfx::Size(kSmallImageSize, kSmallImageSize));
  // The small image view should be added to view hierarchy by the derived
  // class. This ensures that it is on top of other views.
  small_image_view->set_owned_by_client();
  small_image_view_.reset(small_image_view);

  focus_painter_ = views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor, gfx::Insets(0, 1, 3, 2));
}

MessageView::~MessageView() {
}

void MessageView::UpdateWithNotification(const Notification& notification) {
  small_image_view_->SetImage(notification.small_image().AsImageSkia());
  display_source_ = notification.display_source();
}

// static
gfx::Insets MessageView::GetShadowInsets() {
  return gfx::Insets(kShadowBlur / 2 - kShadowOffset,
                     kShadowBlur / 2,
                     kShadowBlur / 2 + kShadowOffset,
                     kShadowBlur / 2);
}

void MessageView::CreateShadowBorder() {
  SetBorder(std::unique_ptr<views::Border>(new views::ShadowBorder(
      gfx::ShadowValue(gfx::Vector2d(0, kShadowOffset), kShadowBlur,
                       message_center::kShadowColor))));
}

bool MessageView::IsCloseButtonFocused() {
  // May be overridden by the owner of the close button.
  return false;
}

void MessageView::RequestFocusOnCloseButton() {
  // May be overridden by the owner of the close button.
}

bool MessageView::IsPinned() {
  return false;
}

void MessageView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_BUTTON;
  state->name = accessible_name_;
}

bool MessageView::OnMousePressed(const ui::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton())
    return false;

  controller_->ClickOnNotification(notification_id_);
  return true;
}

bool MessageView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.flags() != ui::EF_NONE)
    return false;

  if (event.key_code() == ui::VKEY_RETURN) {
    controller_->ClickOnNotification(notification_id_);
    return true;
  } else if ((event.key_code() == ui::VKEY_DELETE ||
              event.key_code() == ui::VKEY_BACK)) {
    controller_->RemoveNotification(notification_id_, true);  // By user.
    return true;
  }

  return false;
}

bool MessageView::OnKeyReleased(const ui::KeyEvent& event) {
  // Space key handling is triggerred at key-release timing. See
  // ui/views/controls/buttons/custom_button.cc for why.
  if (event.flags() != ui::EF_NONE || event.flags() != ui::VKEY_SPACE)
    return false;

  controller_->ClickOnNotification(notification_id_);
  return true;
}

void MessageView::OnPaint(gfx::Canvas* canvas) {
  DCHECK_EQ(this, small_image_view_->parent());
  SlideOutView::OnPaint(canvas);
  views::Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
}

void MessageView::OnFocus() {
  SlideOutView::OnFocus();
  // We paint a focus indicator.
  SchedulePaint();
}

void MessageView::OnBlur() {
  SlideOutView::OnBlur();
  // We paint a focus indicator.
  SchedulePaint();
}

void MessageView::Layout() {
  gfx::Rect content_bounds = GetContentsBounds();

  // Background.
  background_view_->SetBoundsRect(content_bounds);

  gfx::Size small_image_size(small_image_view_->GetPreferredSize());
  gfx::Rect small_image_rect(small_image_size);
  small_image_rect.set_origin(gfx::Point(
      content_bounds.right() - small_image_size.width() - kSmallImagePadding,
      content_bounds.bottom() - small_image_size.height() -
          kSmallImagePadding));
  small_image_view_->SetBoundsRect(small_image_rect);
}

void MessageView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN: {
      SetDrawBackgroundAsActive(true);
      break;
    }
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_END: {
      SetDrawBackgroundAsActive(false);
      break;
    }
    case ui::ET_GESTURE_TAP: {
      SetDrawBackgroundAsActive(false);
      controller_->ClickOnNotification(notification_id_);
      event->SetHandled();
      return;
    }
    default: {
      // Do nothing
    }
  }

  SlideOutView::OnGestureEvent(event);
  // Do not return here by checking handled(). SlideOutView calls SetHandled()
  // even though the scroll gesture doesn't make no (or little) effects on the
  // slide-out behavior. See http://crbug.com/172991

  if (!event->IsScrollGestureEvent() && !event->IsFlingScrollEvent())
    return;

  if (scroller_)
    scroller_->OnGestureEvent(event);
  event->SetHandled();
}

void MessageView::ButtonPressed(views::Button* sender,
                                const ui::Event& event) {
}

void MessageView::OnSlideOut() {
  controller_->RemoveNotification(notification_id_, true);  // By user.
}

void MessageView::SetDrawBackgroundAsActive(bool active) {
  if (!switches::IsTouchFeedbackEnabled())
    return;
  background_view_->background()->
      SetNativeControlColor(active ? kHoveredButtonBackgroundColor :
                                     kNotificationBackgroundColor);
  SchedulePaint();
}

}  // namespace message_center
