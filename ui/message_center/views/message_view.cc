// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_view.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/shadow_util.h"
#include "ui/gfx/shadow_value.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_view_delegate.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace {

#if defined(OS_CHROMEOS)
const int kBorderCorderRadius = 2;
const SkColor kBorderColor = SkColorSetARGB(0x1F, 0x0, 0x0, 0x0);
#else
const int kShadowCornerRadius = 0;
const int kShadowElevation = 2;
#endif

// Creates a text for spoken feedback from the data contained in the
// notification.
base::string16 CreateAccessibleName(
    const message_center::Notification& notification) {
  if (!notification.accessible_name().empty())
    return notification.accessible_name();

  // Fall back to a text constructed from the notification.
  std::vector<base::string16> accessible_lines = {
      notification.title(), notification.message(),
      notification.context_message()};
  std::vector<message_center::NotificationItem> items = notification.items();
  for (size_t i = 0;
       i < items.size() && i < message_center::kNotificationMaximumItems;
       ++i) {
    accessible_lines.push_back(items[i].title + base::ASCIIToUTF16(" ") +
                               items[i].message);
  }
  return base::JoinString(accessible_lines, base::ASCIIToUTF16("\n"));
}

}  // namespace

namespace message_center {

MessageView::MessageView(MessageViewDelegate* delegate,
                         const Notification& notification)
    : delegate_(delegate),
      notification_id_(notification.id()),
      slide_out_controller_(this, this) {
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // Paint to a dedicated layer to make the layer non-opaque.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Create the opaque background that's above the view's shadow.
  background_view_ = new views::View();
  background_view_->SetBackground(
      views::CreateSolidBackground(kNotificationBackgroundColor));
  AddChildView(background_view_);

  focus_painter_ = views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor, gfx::Insets(0, 0, 1, 1));

  UpdateWithNotification(notification);
}

MessageView::~MessageView() {
}

void MessageView::UpdateWithNotification(const Notification& notification) {
  pinned_ = notification.pinned();
  accessible_name_ = CreateAccessibleName(notification);
  slide_out_controller_.set_enabled(!GetPinned());
}

void MessageView::SetIsNested() {
  is_nested_ = true;

#if defined(OS_CHROMEOS)
  SetBorder(views::CreateRoundedRectBorder(kNotificationBorderThickness,
                                           kBorderCorderRadius, kBorderColor));
#else
  const auto& shadow =
      gfx::ShadowDetails::Get(kShadowElevation, kShadowCornerRadius);
  gfx::Insets ninebox_insets = gfx::ShadowValue::GetBlurRegion(shadow.values) +
                               gfx::Insets(kShadowCornerRadius);
  SetBorder(views::CreateBorderPainter(
      std::unique_ptr<views::Painter>(views::Painter::CreateImagePainter(
          shadow.ninebox_image, ninebox_insets)),
      -gfx::ShadowValue::GetMargin(shadow.values)));
#endif
}

void MessageView::SetExpanded(bool expanded) {
  // Not implemented by default.
}

bool MessageView::IsExpanded() const {
  // Not implemented by default.
  return false;
}

void MessageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_BUTTON;
  node_data->AddStringAttribute(
      ui::AX_ATTR_ROLE_DESCRIPTION,
      l10n_util::GetStringUTF8(
          IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
  node_data->SetName(accessible_name_);
}

bool MessageView::OnMousePressed(const ui::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton())
    return false;

  delegate_->ClickOnNotification(notification_id_);
  return true;
}

bool MessageView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.flags() != ui::EF_NONE)
    return false;

  if (event.key_code() == ui::VKEY_RETURN) {
    delegate_->ClickOnNotification(notification_id_);
    return true;
  } else if ((event.key_code() == ui::VKEY_DELETE ||
              event.key_code() == ui::VKEY_BACK)) {
    delegate_->RemoveNotification(notification_id_, true);  // By user.
    return true;
  }

  return false;
}

bool MessageView::OnKeyReleased(const ui::KeyEvent& event) {
  // Space key handling is triggerred at key-release timing. See
  // ui/views/controls/buttons/button.cc for why.
  if (event.flags() != ui::EF_NONE || event.key_code() != ui::VKEY_SPACE)
    return false;

  delegate_->ClickOnNotification(notification_id_);
  return true;
}

void MessageView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  views::Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
}

void MessageView::OnFocus() {
  views::View::OnFocus();
  // We paint a focus indicator.
  SchedulePaint();
}

void MessageView::OnBlur() {
  views::View::OnBlur();
  // We paint a focus indicator.
  SchedulePaint();
}

void MessageView::Layout() {
  views::View::Layout();

  gfx::Rect content_bounds = GetContentsBounds();

  // Background.
  background_view_->SetBoundsRect(content_bounds);
#if defined(OS_CHROMEOS)
  // ChromeOS rounds the corners of the message view. TODO(estade): should we do
  // this for all platforms?
  gfx::Path path;
  constexpr SkScalar kCornerRadius = SkIntToScalar(2);
  path.addRoundRect(gfx::RectToSkRect(background_view_->GetLocalBounds()),
                    kCornerRadius, kCornerRadius);
  background_view_->set_clip_path(path);
#endif
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
      delegate_->ClickOnNotification(notification_id_);
      event->SetHandled();
      return;
    }
    default: {
      // Do nothing
    }
  }

  if (!event->IsScrollGestureEvent() && !event->IsFlingScrollEvent())
    return;

  if (scroller_)
    scroller_->OnGestureEvent(event);
  event->SetHandled();
}

ui::Layer* MessageView::GetSlideOutLayer() {
  return is_nested_ ? layer() : GetWidget()->GetLayer();
}

void MessageView::OnSlideChanged() {}

void MessageView::OnSlideOut() {
  delegate_->RemoveNotification(notification_id_, true);  // By user.
}

bool MessageView::GetPinned() const {
  return pinned_ && !force_disable_pinned_;
}

void MessageView::OnCloseButtonPressed() {
  delegate_->RemoveNotification(notification_id_, true);  // By user.
}

void MessageView::OnSettingsButtonPressed() {
  delegate_->ClickOnSettingsButton(notification_id_);
}

void MessageView::SetDrawBackgroundAsActive(bool active) {
  background_view_->background()->
      SetNativeControlColor(active ? kHoveredButtonBackgroundColor :
                                     kNotificationBackgroundColor);
  SchedulePaint();
}

}  // namespace message_center
