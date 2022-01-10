// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_icons_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/message_center_utils.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"

namespace ash {

namespace {

// Maximum number of notification icons shown in the system tray button.
constexpr int kMaxNotificationIconsShown = 2;
constexpr int kNotificationIconSpacing = 1;

const char kBatteryNotificationNotifierId[] = "ash.battery";
const char kUsbNotificationNotifierId[] = "ash.power";

bool ShouldShowNotification(message_center::Notification* notification) {
  // We don't want to show these notifications since the information is already
  // indicated by another item in tray.
  std::string id = notification->notifier_id().id;
  if (id == kVmCameraMicNotifierId || id == kBatteryNotificationNotifierId ||
      id == kUsbNotificationNotifierId)
    return false;

  // We only show notification icon in the tray if it is either:
  // *   Pinned (generally used for background process such as sharing your
  //     screen, capslock, etc.).
  // *   Critical warning (display failure, disk space critically low, etc.).
  return notification->pinned() ||
         notification->system_notification_warning_level() ==
             message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
}

}  // namespace

NotificationIconTrayItemView::NotificationIconTrayItemView(
    Shelf* shelf,
    NotificationIconsController* controller)
    : TrayItemView(shelf), controller_(controller) {
  CreateImageView();
  image_view()->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(0, kNotificationIconSpacing)));
}

NotificationIconTrayItemView::~NotificationIconTrayItemView() = default;

void NotificationIconTrayItemView::SetNotification(
    message_center::Notification* notification) {
  notification_id_ = notification->id();

  if (!GetWidget())
    return;

  const auto* color_provider = GetColorProvider();
  gfx::Image masked_small_icon = notification->GenerateMaskedSmallIcon(
      kUnifiedTrayIconSize,
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState()),
      color_provider->GetColor(ui::kColorNotificationIconBackground),
      color_provider->GetColor(ui::kColorNotificationIconForeground));
  if (!masked_small_icon.IsEmpty()) {
    image_view()->SetImage(masked_small_icon.AsImageSkia());
  } else {
    image_view()->SetImage(gfx::CreateVectorIcon(
        message_center::kProductIcon, kUnifiedTrayIconSize,
        TrayIconColor(Shell::Get()->session_controller()->GetSessionState())));
  }

  image_view()->SetTooltipText(notification->title());
}

void NotificationIconTrayItemView::Reset() {
  notification_id_ = std::string();
  image_view()->SetImage(gfx::ImageSkia());
  image_view()->SetTooltipText(std::u16string());
}

const std::u16string& NotificationIconTrayItemView::GetAccessibleNameString()
    const {
  if (notification_id_.empty())
    return base::EmptyString16();
  return image_view()->GetTooltipText();
}

const std::string& NotificationIconTrayItemView::GetNotificationId() const {
  return notification_id_;
}

void NotificationIconTrayItemView::HandleLocaleChange() {}

const char* NotificationIconTrayItemView::GetClassName() const {
  return "NotificationIconTrayItemView";
}

void NotificationIconTrayItemView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  controller_->UpdateNotificationIcons();
}

NotificationIconsController::NotificationIconsController(
    UnifiedSystemTray* tray)
    : tray_(tray) {
  system_tray_model_observation_.Observe(tray_->model().get());
  message_center::MessageCenter::Get()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
}

NotificationIconsController::~NotificationIconsController() {
  message_center::MessageCenter::Get()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void NotificationIconsController::AddNotificationTrayItems(
    TrayContainer* tray_container) {
  for (int i = 0; i < kMaxNotificationIconsShown; ++i) {
    tray_items_.push_back(tray_container->AddChildView(
        std::make_unique<NotificationIconTrayItemView>(tray_->shelf(), this)));
  }

  notification_counter_view_ = tray_container->AddChildView(
      std::make_unique<NotificationCounterView>(tray_->shelf(), this));

  quiet_mode_view_ = tray_container->AddChildView(
      std::make_unique<QuietModeView>(tray_->shelf()));

  separator_ = tray_container->AddChildView(
      std::make_unique<SeparatorTrayItemView>(tray_->shelf()));

  OnSystemTrayButtonSizeChanged(tray_->model()->GetSystemTrayButtonSize());
}

bool NotificationIconsController::TrayItemHasNotification() const {
  return first_unused_item_index_ != 0;
}

size_t NotificationIconsController::TrayNotificationIconsCount() const {
  // `first_unused_item_index_` is also the total number of notification icons
  // shown in the tray.
  return first_unused_item_index_;
}

bool NotificationIconsController::ShouldShowNotificationItemsInTray() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  return !message_center::MessageCenter::Get()->IsQuietMode() &&
         session_controller->ShouldShowNotificationTray() &&
         (!session_controller->IsScreenLocked() ||
          AshMessageCenterLockScreenController::IsEnabled());
}

std::u16string NotificationIconsController::GetAccessibleNameString() const {
  if (!TrayItemHasNotification())
    return notification_counter_view_->GetAccessibleNameString();

  std::vector<std::u16string> status;
  status.push_back(l10n_util::GetPluralStringFUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATIONS_IMPORTANT_COUNT_ACCESSIBLE_NAME,
      TrayNotificationIconsCount()));
  for (NotificationIconTrayItemView* tray_item : tray_items_) {
    status.push_back(tray_item->GetAccessibleNameString());
  }
  status.push_back(notification_counter_view_->GetAccessibleNameString());
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATIONS_ICONS_ACCESSIBLE_NAME, status, nullptr);
}

void NotificationIconsController::UpdateNotificationIndicators() {
  notification_counter_view_->Update();
  quiet_mode_view_->Update();
}

void NotificationIconsController::OnSystemTrayButtonSizeChanged(
    UnifiedSystemTrayModel::SystemTrayButtonSize system_tray_size) {
  icons_view_visible_ =
      features::IsScalableStatusAreaEnabled() &&
      system_tray_size != UnifiedSystemTrayModel::SystemTrayButtonSize::kSmall;
  UpdateNotificationIcons();
  UpdateNotificationIndicators();
}

void NotificationIconsController::OnNotificationAdded(const std::string& id) {
  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(id);
  // `notification` is null if it is not visible.
  if (!notification || !ShouldShowNotification(notification))
    return;

  // Reset the notification icons if a notification is added since we don't
  // know the position where its icon should be added.
  UpdateNotificationIcons();
}

void NotificationIconsController::OnNotificationRemoved(const std::string& id,
                                                        bool by_user) {
  // If the notification removed is displayed in an icon, call update to show
  // another notification if needed.
  if (GetNotificationIconShownInTray(id))
    UpdateNotificationIcons();
}

void NotificationIconsController::OnNotificationUpdated(const std::string& id) {
  // A notification update may impact certain notification icon(s) visibility in
  // the tray, so update all notification icons.
  UpdateNotificationIcons();
}

void NotificationIconsController::OnSessionStateChanged(
    session_manager::SessionState state) {
  UpdateNotificationIcons();
  UpdateNotificationIndicators();
  separator_->UpdateColor(state);
}

void NotificationIconsController::UpdateNotificationIcons() {
  const bool should_show_icons =
      icons_view_visible_ && ShouldShowNotificationItemsInTray();

  auto it = tray_items_.begin();
  for (message_center::Notification* notification :
       message_center_utils::GetSortedNotificationsWithOwnView()) {
    if (it == tray_items_.end())
      break;
    if (ShouldShowNotification(notification)) {
      (*it)->SetNotification(notification);
      (*it)->SetVisible(should_show_icons);
      ++it;
    }
  }

  first_unused_item_index_ = std::distance(tray_items_.begin(), it);

  for (; it != tray_items_.end(); ++it) {
    (*it)->Reset();
    (*it)->SetVisible(false);
  }
  separator_->SetVisible(should_show_icons && TrayItemHasNotification());
}

NotificationIconTrayItemView*
NotificationIconsController::GetNotificationIconShownInTray(
    const std::string& id) {
  for (NotificationIconTrayItemView* tray_item : tray_items_) {
    if (tray_item->GetNotificationId() == id)
      return tray_item;
  }
  return nullptr;
}

}  // namespace ash
