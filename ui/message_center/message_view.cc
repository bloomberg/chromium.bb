// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_view.h"

#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
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
      icon_(NULL),
      close_button_(NULL),
      scroller_(NULL),
      gesture_scroll_amount_(0.f) {
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

ui::EventResult MessageView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    list_delegate_->OnNotificationClicked(notification_.id);
    return ui::ER_CONSUMED;
  }

  if (event->type() == ui::ET_GESTURE_LONG_PRESS) {
    ShowMenu(event->location());
    return ui::ER_CONSUMED;
  }

  if (event->type() == ui::ET_SCROLL_FLING_START) {
    // The threshold for the fling velocity is computed empirically.
    // The unit is in pixels/second.
    const float kFlingThresholdForClose = 800.f;
    if (fabsf(event->details().velocity_x()) > kFlingThresholdForClose) {
      SlideOutAndClose(event->details().velocity_x() < 0 ? SLIDE_LEFT :
                       SLIDE_RIGHT);
    } else if (scroller_) {
      RestoreVisualState();
      scroller_->OnGestureEvent(event);
    }
    return ui::ER_CONSUMED;
  }

  if (!event->IsScrollGestureEvent())
    return ui::ER_UNHANDLED;

  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    gesture_scroll_amount_ = 0.f;
  } else if (event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    // The scroll-update events include the incremental scroll amount.
    gesture_scroll_amount_ += event->details().scroll_x();

    gfx::Transform transform;
    transform.SetTranslateX(gesture_scroll_amount_);
    layer()->SetTransform(transform);
    layer()->SetOpacity(
        1.f - std::min(fabsf(gesture_scroll_amount_) / width(), 1.f));

  } else if (event->type() == ui::ET_GESTURE_SCROLL_END) {
    const float kScrollRatioForClosingNotification = 0.5f;
    float scrolled_ratio = fabsf(gesture_scroll_amount_) / width();
    if (scrolled_ratio >= kScrollRatioForClosingNotification)
      SlideOutAndClose(gesture_scroll_amount_ < 0 ? SLIDE_LEFT : SLIDE_RIGHT);
    else
      RestoreVisualState();
  }

  if (scroller_)
    scroller_->OnGestureEvent(event);
  return ui::ER_CONSUMED;
}

void MessageView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  if (sender == close_button_)
    list_delegate_->SendRemoveNotification(notification_.id);
}

void MessageView::OnImplicitAnimationsCompleted() {
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

void MessageView::RestoreVisualState() {
  // Restore the layer state.
  const int kSwipeRestoreDurationMS = 150;
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kSwipeRestoreDurationMS));
  layer()->SetTransform(gfx::Transform());
  layer()->SetOpacity(1.f);
}

void MessageView::SlideOutAndClose(SlideDirection direction) {
  const int kSwipeOutTotalDurationMS = 150;
  int swipe_out_duration = kSwipeOutTotalDurationMS * layer()->opacity();
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(swipe_out_duration));
  settings.AddObserver(this);

  gfx::Transform transform;
  transform.SetTranslateX(direction == SLIDE_LEFT ? -width() : width());
  layer()->SetTransform(transform);
  layer()->SetOpacity(0.f);
}

}  // namespace message_center
