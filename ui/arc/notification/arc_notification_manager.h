// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_MANAGER_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_MANAGER_H_

#include <map>
#include <string>

#include "base/containers/scoped_ptr_hash_map.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/notifications.mojom.h"
#include "components/signin/core/account_id/account_id.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class ArcNotificationItem;

class ArcNotificationManager : public ArcBridgeService::Observer,
                               public NotificationsHost {
 public:
  ArcNotificationManager(ArcBridgeService* bridge_service,
                         const AccountId& main_profile_id);
  ~ArcNotificationManager() override;

  // ArcBridgeService::Observer implementation:
  void OnNotificationsInstanceReady() override;

  // NotificationsHost implementation:
  void OnNotificationPosted(ArcNotificationDataPtr data) override;
  void OnNotificationRemoved(const mojo::String& key) override;

  // Methods called from ArcNotificationItem:
  void SendNotificationRemovedFromChrome(const std::string& key);
  void SendNotificationClickedOnChrome(const std::string& key);

 private:
  ArcBridgeService* const arc_bridge_;
  const AccountId main_profile_id_;

  using ItemMap =
      base::ScopedPtrHashMap<std::string, scoped_ptr<ArcNotificationItem>>;
  ItemMap items_;

  mojo::Binding<arc::NotificationsHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationManager);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_MANAGER_H_
