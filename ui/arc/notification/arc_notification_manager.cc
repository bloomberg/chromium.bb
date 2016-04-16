// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_notification_manager.h"

#include "ash/shell.h"
#include "ash/system/toast/toast_manager.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "ui/arc/notification/arc_notification_item.h"

namespace arc {

ArcNotificationManager::ArcNotificationManager(ArcBridgeService* bridge_service,
                                               const AccountId& main_profile_id)
    : ArcNotificationManager(bridge_service,
                             main_profile_id,
                             message_center::MessageCenter::Get()) {}

ArcNotificationManager::ArcNotificationManager(
    ArcBridgeService* bridge_service,
    const AccountId& main_profile_id,
    message_center::MessageCenter* message_center)
    : ArcService(bridge_service),
      main_profile_id_(main_profile_id),
      message_center_(message_center),
      binding_(this) {
  arc_bridge_service()->AddObserver(this);
}

ArcNotificationManager::~ArcNotificationManager() {
  arc_bridge_service()->RemoveObserver(this);
}

void ArcNotificationManager::OnNotificationsInstanceReady() {
  DCHECK(!ready_);

  auto notifications_instance = arc_bridge_service()->notifications_instance();
  if (!notifications_instance) {
    VLOG(2) << "Request to refresh app list when bridge service is not ready.";
    return;
  }

  notifications_instance->Init(binding_.CreateInterfacePtrAndBind());
  ready_ = true;
}

void ArcNotificationManager::OnNotificationsInstanceClosed() {
  DCHECK(ready_);
  while (!items_.empty()) {
    auto it = items_.begin();
    std::unique_ptr<ArcNotificationItem> item = std::move(it->second);
    items_.erase(it);
    item->OnClosedFromAndroid(false /* by_user */);
  }
  ready_ = false;
}

void ArcNotificationManager::OnNotificationPosted(
    mojom::ArcNotificationDataPtr data) {
  const std::string& key = data->key;
  auto it = items_.find(key);
  if (it == items_.end()) {
    // Show a notification on the primary loged-in user's desktop.
    // TODO(yoshiki): Reconsider when ARC supports multi-user.
    ArcNotificationItem* item =
        new ArcNotificationItem(this, message_center_, key, main_profile_id_);
    // TODO(yoshiki): Use emplacement for performance when it's available.
    auto result = items_.insert(std::make_pair(key, base::WrapUnique(item)));
    DCHECK(result.second);
    it = result.first;
  }
  it->second->UpdateWithArcNotificationData(*data);
}

void ArcNotificationManager::OnNotificationRemoved(const mojo::String& key) {
  auto it = items_.find(key.get());
  if (it == items_.end()) {
    VLOG(3) << "Android requests to remove a notification (key: " << key
            << "), but it is already gone.";
    return;
  }

  std::unique_ptr<ArcNotificationItem> item = std::move(it->second);
  items_.erase(it);
  item->OnClosedFromAndroid(true /* by_user */);
}

void ArcNotificationManager::SendNotificationRemovedFromChrome(
    const std::string& key) {
  auto it = items_.find(key);
  if (it == items_.end()) {
    VLOG(3) << "Chrome requests to remove a notification (key: " << key
            << "), but it is already gone.";
    return;
  }

  // The removed ArcNotificationItem needs to live in this scope, since the
  // given argument |key| may be a part of the removed item.
  std::unique_ptr<ArcNotificationItem> item = std::move(it->second);
  items_.erase(it);

  auto notifications_instance = arc_bridge_service()->notifications_instance();

  // On shutdown, the ARC channel may quit earlier then notifications.
  if (!notifications_instance) {
    VLOG(2) << "ARC Notification (key: " << key
            << ") is closed, but the ARC channel has already gone.";
    return;
  }

  notifications_instance->SendNotificationEventToAndroid(
      key, mojom::ArcNotificationEvent::CLOSED);
}

void ArcNotificationManager::SendNotificationClickedOnChrome(
    const std::string& key) {
  if (items_.find(key) == items_.end()) {
    VLOG(3) << "Chrome requests to fire a click event on notification (key: "
            << key << "), but it is gone.";
    return;
  }

  auto notifications_instance = arc_bridge_service()->notifications_instance();

  // On shutdown, the ARC channel may quit earlier then notifications.
  if (!notifications_instance) {
    VLOG(2) << "ARC Notification (key: " << key
            << ") is clicked, but the ARC channel has already gone.";
    return;
  }

  notifications_instance->SendNotificationEventToAndroid(
      key, mojom::ArcNotificationEvent::BODY_CLICKED);
}

void ArcNotificationManager::SendNotificationButtonClickedOnChrome(
    const std::string& key, int button_index) {
  if (items_.find(key) == items_.end()) {
    VLOG(3) << "Chrome requests to fire a click event on notification (key: "
            << key << "), but it is gone.";
    return;
  }

  auto notifications_instance = arc_bridge_service()->notifications_instance();

  // On shutdown, the ARC channel may quit earlier then notifications.
  if (!notifications_instance) {
    VLOG(2) << "ARC Notification (key: " << key
            << ")'s button is clicked, but the ARC channel has already gone.";
    return;
  }

  arc::mojom::ArcNotificationEvent command;
  switch (button_index) {
    case 0:
      command = mojom::ArcNotificationEvent::BUTTON_1_CLICKED;
      break;
    case 1:
      command = mojom::ArcNotificationEvent::BUTTON_2_CLICKED;
      break;
    case 2:
      command = mojom::ArcNotificationEvent::BUTTON_3_CLICKED;
      break;
    case 3:
      command = mojom::ArcNotificationEvent::BUTTON_4_CLICKED;
      break;
    case 4:
      command = mojom::ArcNotificationEvent::BUTTON_5_CLICKED;
      break;
    default:
      VLOG(3) << "Invalid button index (key: " << key << ", index: " <<
          button_index << ").";
      return;
  }

  notifications_instance->SendNotificationEventToAndroid(key, command);
}

void ArcNotificationManager::OnToastPosted(mojom::ArcToastDataPtr data) {
  ash::Shell::GetInstance()->toast_manager()->Show(data->text, data->duration);
}

void ArcNotificationManager::OnToastCancelled(mojom::ArcToastDataPtr data) {
  // TODO(yoshiki): Implement cancel.
}

}  // namespace arc
