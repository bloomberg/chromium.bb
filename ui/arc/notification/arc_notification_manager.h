// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_MANAGER_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"
#include "components/arc/common/notifications.mojom.h"
#include "components/arc/connection_holder.h"
#include "components/arc/connection_observer.h"
#include "ui/message_center/message_center.h"

namespace arc {

class ArcNotificationItem;

class ArcNotificationManager
    : public ConnectionObserver<mojom::NotificationsInstance>,
      public mojom::NotificationsHost {
 public:
  // Sets the factory function to create ARC notification views. Exposed for
  // testing.
  static void SetCustomNotificationViewFactory();

  ArcNotificationManager(const AccountId& main_profile_id,
                         message_center::MessageCenter* message_center);

  ~ArcNotificationManager() override;

  void SetInstance(mojom::NotificationsInstancePtr instance);

  ConnectionHolder<mojom::NotificationsInstance, mojom::NotificationsHost>*
  GetConnectionHolderForTest();

  using GetAppIdResponseCallback =
      base::OnceCallback<void(const std::string& app_id)>;
  using GetAppIdCallback =
      base::RepeatingCallback<void(const std::string& package_name,
                                   GetAppIdResponseCallback callback)>;
  void set_get_app_id_callback(const GetAppIdCallback& get_app_id_callback) {
    get_app_id_callback_ = get_app_id_callback;
  }

  // ConnectionObserver<mojom::NotificationsInstance> implementation:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // mojom::NotificationsHost implementation:
  void OnNotificationPosted(mojom::ArcNotificationDataPtr data) override;
  void OnNotificationUpdated(mojom::ArcNotificationDataPtr data) override;
  void OnNotificationRemoved(const std::string& key) override;

  // Methods called from ArcNotificationItem:
  void SendNotificationRemovedFromChrome(const std::string& key);
  void SendNotificationClickedOnChrome(const std::string& key);
  void SendNotificationButtonClickedOnChrome(const std::string& key,
                                             int button_index);
  void CreateNotificationWindow(const std::string& key);
  void CloseNotificationWindow(const std::string& key);
  void OpenNotificationSettings(const std::string& key);
  bool IsOpeningSettingsSupported() const;
  void SendNotificationToggleExpansionOnChrome(const std::string& key);

 private:
  // Helper class to own MojoChannel and ConnectionHolder.
  class InstanceOwner;

  bool ShouldIgnoreNotification(mojom::ArcNotificationData* data);

  // Calls |get_app_id_callback_| to retrieve the app id. |callback| will be
  // invoked with the app id or an empty string.
  void GetAppId(const std::string& package_name,
                GetAppIdResponseCallback callback) const;

  // Invoked when |get_app_id_callback_| gets back the app id.
  void OnGotAppId(mojom::ArcNotificationDataPtr data,
                  const std::string& app_id);

  const AccountId main_profile_id_;
  message_center::MessageCenter* const message_center_;

  using ItemMap =
      std::unordered_map<std::string, std::unique_ptr<ArcNotificationItem>>;
  ItemMap items_;

  GetAppIdCallback get_app_id_callback_;

  bool ready_ = false;

  std::unique_ptr<InstanceOwner> instance_owner_;

  base::WeakPtrFactory<ArcNotificationManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationManager);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_MANAGER_H_
