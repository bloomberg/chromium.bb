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
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/painter.h"
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
  MenuModel(message_center::MessageViewController* controller,
            message_center::NotifierId notifier_id,
            const string16& display_source);
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
  message_center::MessageViewController* controller_;
  message_center::NotifierId notifier_id_;
  DISALLOW_COPY_AND_ASSIGN(MenuModel);
};

MenuModel::MenuModel(message_center::MessageViewController* controller,
                     message_center::NotifierId notifier_id,
                     const string16& display_source)
    : ui::SimpleMenuModel(this),
      controller_(controller),
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
      controller_->DisableNotificationsFromThisSource(notifier_id_);
      break;
    case kShowSettingsCommand:
        controller_->ShowNotifierSettingsBubble();
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

namespace message_center {

class MessageViewContextMenuController : public views::ContextMenuController {
 public:
  MessageViewContextMenuController(MessageViewController* controller,
                                   const NotifierId& notifier_id,
                                   const string16& display_source);
  virtual ~MessageViewContextMenuController();

 protected:
  // Overridden from views::ContextMenuController:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& point,
                                      ui::MenuSourceType source_type) OVERRIDE;

  MessageViewController* controller_;  // Weak, owns us.
  NotifierId notifier_id_;
  string16 display_source_;
};

MessageViewContextMenuController::MessageViewContextMenuController(
    MessageViewController* controller,
    const NotifierId& notifier_id,
    const string16& display_source)
    : controller_(controller),
      notifier_id_(notifier_id),
      display_source_(display_source) {
}

MessageViewContextMenuController::~MessageViewContextMenuController() {
}

void MessageViewContextMenuController::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  MenuModel menu_model(controller_, notifier_id_, display_source_);
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

MessageView::MessageView(MessageViewController* controller,
                         const std::string& notification_id,
                         const NotifierId& notifier_id,
                         const string16& display_source)
    : controller_(controller),
      notification_id_(notification_id),
      notifier_id_(notifier_id),
      context_menu_controller_(
        new MessageViewContextMenuController(controller,
                                             notifier_id,
                                             display_source)),
      scroller_(NULL) {
  SetFocusable(true);
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

  focus_painter_ = views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor,
      gfx::Insets(0, 1, 3, 2)).Pass();
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

void MessageView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    controller_->ClickOnNotification(notification_id_);
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

void MessageView::ButtonPressed(views::Button* sender,
                                const ui::Event& event) {
  if (sender == close_button()) {
    controller_->RemoveNotification(notification_id_, true);  // By user.
  }
}

void MessageView::OnSlideOut() {
  controller_->RemoveNotification(notification_id_, true);  // By user.
}

}  // namespace message_center
