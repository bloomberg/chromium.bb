// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_MANAGER_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "components/arc/common/notifications.mojom.h"
#include "components/arc/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/message_center/message_center.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;
class ArcNotificationItem;

class ArcNotificationManager
    : public KeyedService,
      public ConnectionObserver<mojom::NotificationsInstance>,
      public mojom::NotificationsHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcNotificationManager* GetForBrowserContext(
      content::BrowserContext* context);

  // Returns a created instance for testing.
  static std::unique_ptr<ArcNotificationManager> CreateForTesting(
      ArcBridgeService* bridge_service,
      const AccountId& main_profile_id,
      message_center::MessageCenter* message_center);

  // Sets the factory function to create ARC notification views. Exposed for
  // testing.
  static void SetCustomNotificationViewFactory();

  // TODO(hidehiko): Make ctor private to enforce all service users should
  // use GetForBrowserContext().
  ArcNotificationManager(content::BrowserContext* context,
                         ArcBridgeService* bridge_service);

  ~ArcNotificationManager() override;

  // ConnectionObserver<mojom::NotificationsInstance> implementation:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // mojom::NotificationsHost implementation:
  void OnNotificationPosted(mojom::ArcNotificationDataPtr data) override;
  void OnNotificationRemoved(const std::string& key) override;
  void OnToastPosted(mojom::ArcToastDataPtr data) override;
  void OnToastCancelled(mojom::ArcToastDataPtr data) override;

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
  ArcNotificationManager(ArcBridgeService* bridge_service,
                         const AccountId& main_profile_id,
                         message_center::MessageCenter* message_center);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  const AccountId main_profile_id_;
  message_center::MessageCenter* const message_center_;

  using ItemMap =
      std::unordered_map<std::string, std::unique_ptr<ArcNotificationItem>>;
  ItemMap items_;

  bool ready_ = false;

  mojo::Binding<mojom::NotificationsHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationManager);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_MANAGER_H_
