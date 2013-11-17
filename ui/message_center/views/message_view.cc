// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_view.h"

#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/shadow_border.h"
#include "ui/views/widget/widget.h"

namespace {

const int kCloseIconTopPadding = 5;
const int kCloseIconRightPadding = 5;

const int kShadowOffset = 1;
const int kShadowBlur = 4;

// Menu constants
const int kTogglePermissionCommand = 0;
const int kShowSettingsCommand = 1;

// A dropdown menu for notifications.
class MenuModel : public ui::SimpleMenuModel,
                  public ui::SimpleMenuModel::Delegate {
 public:
  MenuModel(message_center::MessageCenter* message_center,
            message_center::MessageCenterTray* tray,
            const std::string& notification_id,
            const string16& display_source,
            const message_center::NotifierId& notifier_id);
  virtual ~MenuModel();

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE;
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  message_center::MessageCenter* message_center_;  // Weak reference.
  message_center::MessageCenterTray* tray_;  // Weak reference.
  std::string notification_id_;
  message_center::NotifierId notifier_id_;

  DISALLOW_COPY_AND_ASSIGN(MenuModel);
};

MenuModel::MenuModel(message_center::MessageCenter* message_center,
                     message_center::MessageCenterTray* tray,
                     const std::string& notification_id,
                     const string16& display_source,
                     const message_center::NotifierId& notifier_id)
    : ui::SimpleMenuModel(this),
      message_center_(message_center),
      tray_(tray),
      notification_id_(notification_id),
      notifier_id_(notifier_id) {
  // Add 'disable notifications' menu item.
  if (!display_source.empty()) {
    AddItem(kTogglePermissionCommand,
            l10n_util::GetStringFUTF16(IDS_MESSAGE_CENTER_NOTIFIER_DISABLE,
                                       display_source));
  }
  // Add settings menu item.
  AddItem(kShowSettingsCommand,
          l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_SETTINGS));
}

MenuModel::~MenuModel() {
}

bool MenuModel::IsItemForCommandIdDynamic(int command_id) const {
  return false;
}

bool MenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool MenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool MenuModel::GetAcceleratorForCommandId(int command_id,
                                           ui::Accelerator* accelerator) {
  return false;
}

void MenuModel::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case kTogglePermissionCommand:
      message_center_->DisableNotificationsByNotifier(notifier_id_);
      break;
    case kShowSettingsCommand:
      // |tray_| may be NULL in tests.
      if (tray_)
        tray_->ShowNotifierSettingsBubble();
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

namespace message_center {

class MessageViewContextMenuController : public views::ContextMenuController {
 public:
  MessageViewContextMenuController(
      MessageCenter* message_center,
      MessageCenterTray* tray,
      const Notification& notification);
  virtual ~MessageViewContextMenuController();

 protected:
  // Overridden from views::ContextMenuController:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& point,
                                      ui::MenuSourceType source_type) OVERRIDE;

  MessageCenter* message_center_;  // Weak reference.
  MessageCenterTray* tray_;  // Weak reference.
  std::string notification_id_;
  string16 display_source_;
  NotifierId notifier_id_;
};

MessageViewContextMenuController::MessageViewContextMenuController(
    MessageCenter* message_center,
    MessageCenterTray* tray,
    const Notification& notification)
    : message_center_(message_center),
      tray_(tray),
      notification_id_(notification.id()),
      display_source_(notification.display_source()),
      notifier_id_(notification.notifier_id()) {
}

MessageViewContextMenuController::~MessageViewContextMenuController() {
}

void MessageViewContextMenuController::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  MenuModel menu_model(message_center_, tray_, notification_id_,
                       display_source_, notifier_id_);
  if (menu_model.GetItemCount() == 0)
    return;

  views::MenuRunner menu_runner(&menu_model);

  ignore_result(menu_runner.RunMenuAt(
      source->GetWidget()->GetTopLevelWidget(),
      NULL,
      gfx::Rect(point, gfx::Size()),
      views::MenuItemView::TOPRIGHT,
      source_type,
      views::MenuRunner::HAS_MNEMONICS));
}

MessageView::MessageView(const Notification& notification,
                         MessageCenter* message_center,
                         MessageCenterTray* tray)
    : message_center_(message_center),
      notification_id_(notification.id()),
      context_menu_controller_(new MessageViewContextMenuController(
          message_center, tray, notification)),
      scroller_(NULL) {
  set_focusable(true);
  set_context_menu_controller(context_menu_controller_.get());

  PaddedButton *close = new PaddedButton(this);
  close->SetPadding(-kCloseIconRightPadding, kCloseIconTopPadding);
  close->SetNormalImage(IDR_NOTIFICATION_CLOSE);
  close->SetHoveredImage(IDR_NOTIFICATION_CLOSE_HOVER);
  close->SetPressedImage(IDR_NOTIFICATION_CLOSE_PRESSED);
  close->set_owned_by_client();
  close->set_animate_on_state_change(false);
  close->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_ACCESSIBLE_NAME));
  close_button_.reset(close);
}

MessageView::MessageView() {
}

MessageView::~MessageView() {
}

// static
gfx::Insets MessageView::GetShadowInsets() {
  return gfx::Insets(kShadowBlur / 2 - kShadowOffset,
                     kShadowBlur / 2,
                     kShadowBlur / 2 + kShadowOffset,
                     kShadowBlur / 2);
}

void MessageView::CreateShadowBorder() {
  set_border(new views::ShadowBorder(kShadowBlur,
                                     message_center::kShadowColor,
                                     kShadowOffset,  // Vertical offset.
                                     0));            // Horizontal offset.
}

bool MessageView::IsCloseButtonFocused() {
  views::FocusManager* focus_manager = GetFocusManager();
  return focus_manager && focus_manager->GetFocusedView() == close_button();
}

void MessageView::RequestFocusOnCloseButton() {
  close_button_->RequestFocus();
}

void MessageView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = accessible_name_;
}

bool MessageView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    message_center_->ClickOnNotification(notification_id_);
    return true;
  }
  return false;
}

bool MessageView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.flags() != ui::EF_NONE)
    return false;

  if (event.key_code() == ui::VKEY_RETURN) {
    message_center_->ClickOnNotification(notification_id_);
    return true;
  } else if ((event.key_code() == ui::VKEY_DELETE ||
              event.key_code() == ui::VKEY_BACK)) {
    message_center_->RemoveNotification(notification_id_, true);  // By user.
    return true;
  }

  return false;
}

bool MessageView::OnKeyReleased(const ui::KeyEvent& event) {
  // Space key handling is triggerred at key-release timing. See
  // ui/views/controls/buttons/custom_button.cc for why.
  if (event.flags() != ui::EF_NONE || event.flags() != ui::VKEY_SPACE)
    return false;

  message_center_->ClickOnNotification(notification_id_);
  return true;
}

void MessageView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    message_center_->ClickOnNotification(notification_id_);
    event->SetHandled();
    return;
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

void MessageView::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus()) {
    canvas->DrawRect(gfx::Rect(1, 0, width() - 2, height() - 2),
                     message_center::kFocusBorderColor);
  }
}

void MessageView::ButtonPressed(views::Button* sender,
                                const ui::Event& event) {
  if (sender == close_button()) {
    message_center_->RemoveNotification(notification_id_, true);  // By user.
  }
}

void MessageView::OnSlideOut() {
  message_center_->RemoveNotification(notification_id_, true);  // By user.
}

}  // namespace message_center
