// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_view.h"

#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/widget/widget.h"

namespace message_center {

// Menu constants
const int kTogglePermissionCommand = 0;
const int kToggleExtensionCommand = 1;
const int kShowSettingsCommand = 2;

// A dropdown menu for notifications.
class WebNotificationMenuModel : public ui::SimpleMenuModel,
                                 public ui::SimpleMenuModel::Delegate {
 public:
  WebNotificationMenuModel(NotificationList::Delegate* list_delegate,
                           const NotificationList::Notification& notification)
      : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
        list_delegate_(list_delegate),
        notification_(notification) {
    // Add 'disable notifications' menu item.
    if (!notification.extension_id.empty()) {
      AddItem(kToggleExtensionCommand,
              GetLabelForCommandId(kToggleExtensionCommand));
    } else if (!notification.display_source.empty()) {
      AddItem(kTogglePermissionCommand,
              GetLabelForCommandId(kTogglePermissionCommand));
    }
    // Add settings menu item.
    if (!notification.display_source.empty()) {
      AddItem(kShowSettingsCommand,
              GetLabelForCommandId(kShowSettingsCommand));
    }
  }

  virtual ~WebNotificationMenuModel() {
  }

  // Overridden from ui::SimpleMenuModel:
  virtual string16 GetLabelForCommandId(int command_id) const OVERRIDE {
    switch (command_id) {
      case kToggleExtensionCommand:
        return l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_EXTENSIONS_DISABLE);
      case kTogglePermissionCommand:
        return l10n_util::GetStringFUTF16(IDS_MESSAGE_CENTER_SITE_DISABLE,
                                          notification_.display_source);
      case kShowSettingsCommand:
        return l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_SETTINGS);
      default:
        NOTREACHED();
    }
    return string16();
  }

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return false;
  }

  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    return false;
  }

  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE {
    return false;
  }

  virtual void ExecuteCommand(int command_id) OVERRIDE {
    switch (command_id) {
      case kToggleExtensionCommand:
        list_delegate_->DisableNotificationByExtension(notification_.id);
        break;
      case kTogglePermissionCommand:
        list_delegate_->DisableNotificationByUrl(notification_.id);
        break;
      case kShowSettingsCommand:
        list_delegate_->ShowNotificationSettings(notification_.id);
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  NotificationList::Delegate* list_delegate_;
  NotificationList::Notification notification_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationMenuModel);
};

MessageView::MessageView(
    NotificationList::Delegate* list_delegate,
    const NotificationList::Notification& notification)
    : list_delegate_(list_delegate),
      notification_(notification),
      close_button_(NULL),
      scroller_(NULL) {
  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(
      views::CustomButton::STATE_NORMAL,
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(IDR_MESSAGE_CLOSE));
  close_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                   views::ImageButton::ALIGN_MIDDLE);
}

MessageView::MessageView() {
}

MessageView::~MessageView() {
}

bool MessageView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.flags() & ui::EF_RIGHT_MOUSE_BUTTON) {
    ShowMenu(event.location());
    return true;
  }
  list_delegate_->OnNotificationClicked(notification_.id);
  return true;
}

void MessageView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    list_delegate_->OnNotificationClicked(notification_.id);
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_LONG_PRESS) {
    ShowMenu(event->location());
    event->SetHandled();
    return;
  }

  SlideOutView::OnGestureEvent(event);
  if (event->handled())
    return;

  if (!event->IsScrollGestureEvent())
    return;

  if (scroller_)
    scroller_->OnGestureEvent(event);
  event->SetHandled();
}

void MessageView::ButtonPressed(views::Button* sender,
                                const ui::Event& event) {
  if (sender == close_button_)
    list_delegate_->SendRemoveNotification(notification_.id);
}

void MessageView::ShowMenu(gfx::Point screen_location) {
  WebNotificationMenuModel menu_model(list_delegate_, notification_);
  if (menu_model.GetItemCount() == 0)
    return;

  views::MenuModelAdapter menu_model_adapter(&menu_model);
  views::MenuRunner menu_runner(menu_model_adapter.CreateMenu());

  views::View::ConvertPointToScreen(this, &screen_location);
  ignore_result(menu_runner.RunMenuAt(
      GetWidget()->GetTopLevelWidget(),
      NULL,
      gfx::Rect(screen_location, gfx::Size()),
      views::MenuItemView::TOPRIGHT,
      views::MenuRunner::HAS_MNEMONICS));
}

void MessageView::OnSlideOut() {
  list_delegate_->SendRemoveNotification(notification_.id);
}

}  // namespace message_center
