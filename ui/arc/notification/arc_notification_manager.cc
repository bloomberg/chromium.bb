// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_notification_manager.h"

#include "base/stl_util.h"
#include "ui/arc/notification/arc_notification_item.h"

namespace arc {

ArcNotificationManager::ArcNotificationManager(ArcBridgeService* arc_bridge,
                                               const AccountId& main_profile_id)
    : arc_bridge_(arc_bridge),
      main_profile_id_(main_profile_id),
      binding_(this) {
  // This must be initialized after ArcBridgeService.
  DCHECK(arc_bridge_);
  DCHECK_EQ(arc_bridge_, ArcBridgeService::Get());
  arc_bridge_->AddObserver(this);
}

ArcNotificationManager::~ArcNotificationManager() {
  // This should be free'd before ArcBridgeService.
  DCHECK(ArcBridgeService::Get());
  DCHECK_EQ(arc_bridge_, ArcBridgeService::Get());
  arc_bridge_->RemoveObserver(this);
}

void ArcNotificationManager::OnNotificationsInstanceReady() {
  NotificationsInstance* notifications_instance =
      arc_bridge_->notifications_instance();
  if (!notifications_instance) {
    VLOG(2) << "Request to refresh app list when bridge service is not ready.";
    return;
  }

  NotificationsHostPtr host;
  binding_.Bind(mojo::GetProxy(&host));
  notifications_instance->Init(std::move(host));
}

void ArcNotificationManager::OnNotificationPosted(ArcNotificationDataPtr data) {
  ArcNotificationItem* item = items_.get(data->key);
  if (!item) {
    // Show a notification on the primary loged-in user's desktop.
    // TODO(yoshiki): Reconsider when ARC supports multi-user.
    item = new ArcNotificationItem(this, message_center::MessageCenter::Get(),
                                   data->key, main_profile_id_);
    items_.set(data->key, make_scoped_ptr(item));
  }
  item->UpdateWithArcNotificationData(*data);
}

void ArcNotificationManager::OnNotificationRemoved(const mojo::String& key) {
  ItemMap::iterator it = items_.find(key.get());
  if (it == items_.end()) {
    VLOG(3) << "Android requests to remove a notification (key: " << key
            << "), but it is already gone.";
    return;
  }

  scoped_ptr<ArcNotificationItem> item(items_.take_and_erase(it));
  item->OnClosedFromAndroid();
}

void ArcNotificationManager::SendNotificationRemovedFromChrome(
    const std::string& key) {
  ItemMap::iterator it = items_.find(key);
  if (it == items_.end()) {
    VLOG(3) << "Chrome requests to remove a notification (key: " << key
            << "), but it is already gone.";
    return;
  }

  scoped_ptr<ArcNotificationItem> item(items_.take_and_erase(it));

  arc_bridge_->notifications_instance()->SendNotificationEventToAndroid(
      key, ARC_NOTIFICATION_EVENT_CLOSED);
}

void ArcNotificationManager::SendNotificationClickedOnChrome(
    const std::string& key) {
  if (!items_.contains(key)) {
    VLOG(3) << "Chrome requests to fire a click event on notification (key: "
            << key << "), but it is gone.";
    return;
  }

  arc_bridge_->notifications_instance()->SendNotificationEventToAndroid(
      key, ARC_NOTIFICATION_EVENT_BODY_CLICKED);
}

}  // namespace arc
