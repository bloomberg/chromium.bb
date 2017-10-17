// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_notification_manager.h"

#include <memory>
#include <utility>

#include "ash/shell.h"
#include "ash/system/toast/toast_manager.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ui/arc/notification/arc_notification_item_impl.h"

namespace arc {
namespace {

// Singleton factory for ArcNotificationManager.
class ArcNotificationManagerFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcNotificationManager,
          ArcNotificationManagerFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcNotificationManagerFactory";

  static ArcNotificationManagerFactory* GetInstance() {
    return base::Singleton<ArcNotificationManagerFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcNotificationManagerFactory>;
  ArcNotificationManagerFactory() = default;
  ~ArcNotificationManagerFactory() override = default;
};

}  // namespace

// static
ArcNotificationManager* ArcNotificationManager::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcNotificationManagerFactory::GetForBrowserContext(context);
}

// static
std::unique_ptr<ArcNotificationManager>
ArcNotificationManager::CreateForTesting(
    ArcBridgeService* bridge_service,
    const AccountId& main_profile_id,
    message_center::MessageCenter* message_center) {
  // MakeUnique cannot be used because the used ctor is private.
  return base::WrapUnique(new ArcNotificationManager(
      bridge_service, main_profile_id, message_center));
}

ArcNotificationManager::ArcNotificationManager(content::BrowserContext* context,
                                               ArcBridgeService* bridge_service)
    : ArcNotificationManager(bridge_service,
                             ArcServiceManager::Get()->account_id(),
                             message_center::MessageCenter::Get()) {}

ArcNotificationManager::ArcNotificationManager(
    ArcBridgeService* bridge_service,
    const AccountId& main_profile_id,
    message_center::MessageCenter* message_center)
    : arc_bridge_service_(bridge_service),
      main_profile_id_(main_profile_id),
      message_center_(message_center),
      binding_(this) {
  arc_bridge_service_->notifications()->AddObserver(this);
}

ArcNotificationManager::~ArcNotificationManager() {
  arc_bridge_service_->notifications()->RemoveObserver(this);
}

void ArcNotificationManager::OnInstanceReady() {
  DCHECK(!ready_);

  auto* notifications_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->notifications(), Init);
  DCHECK(notifications_instance);

  mojom::NotificationsHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  notifications_instance->Init(std::move(host_proxy));
  ready_ = true;
}

void ArcNotificationManager::OnInstanceClosed() {
  DCHECK(ready_);
  while (!items_.empty()) {
    auto it = items_.begin();
    std::unique_ptr<ArcNotificationItem> item = std::move(it->second);
    items_.erase(it);
    item->OnClosedFromAndroid();
  }
  ready_ = false;
}

void ArcNotificationManager::OnNotificationPosted(
    mojom::ArcNotificationDataPtr data) {
  const std::string& key = data->key;
  auto it = items_.find(key);
  if (it == items_.end()) {
    // Show a notification on the primary logged-in user's desktop.
    // TODO(yoshiki): Reconsider when ARC supports multi-user.
    auto item = std::make_unique<ArcNotificationItemImpl>(
        this, message_center_, key, main_profile_id_);
    // TODO(yoshiki): Use emplacement for performance when it's available.
    auto result = items_.insert(std::make_pair(key, std::move(item)));
    DCHECK(result.second);
    it = result.first;
  }
  it->second->OnUpdatedFromAndroid(std::move(data));
}

void ArcNotificationManager::OnNotificationRemoved(const std::string& key) {
  auto it = items_.find(key);
  if (it == items_.end()) {
    VLOG(3) << "Android requests to remove a notification (key: " << key
            << "), but it is already gone.";
    return;
  }

  std::unique_ptr<ArcNotificationItem> item = std::move(it->second);
  items_.erase(it);
  item->OnClosedFromAndroid();
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

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->notifications(), SendNotificationEventToAndroid);

  // On shutdown, the ARC channel may quit earlier than notifications.
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

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->notifications(), SendNotificationEventToAndroid);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "ARC Notification (key: " << key
            << ") is clicked, but the ARC channel has already gone.";
    return;
  }

  notifications_instance->SendNotificationEventToAndroid(
      key, mojom::ArcNotificationEvent::BODY_CLICKED);
}

void ArcNotificationManager::SendNotificationButtonClickedOnChrome(
    const std::string& key,
    int button_index) {
  if (items_.find(key) == items_.end()) {
    VLOG(3) << "Chrome requests to fire a click event on notification (key: "
            << key << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->notifications(), SendNotificationEventToAndroid);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "ARC Notification (key: " << key
            << ")'s button is clicked, but the ARC channel has already gone.";
    return;
  }

  mojom::ArcNotificationEvent command;
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
      VLOG(3) << "Invalid button index (key: " << key
              << ", index: " << button_index << ").";
      return;
  }

  notifications_instance->SendNotificationEventToAndroid(key, command);
}

void ArcNotificationManager::CreateNotificationWindow(const std::string& key) {
  if (items_.find(key) == items_.end()) {
    VLOG(3) << "Chrome requests to create window on notification (key: " << key
            << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->notifications(), CreateNotificationWindow);
  if (!notifications_instance)
    return;

  notifications_instance->CreateNotificationWindow(key);
}

void ArcNotificationManager::CloseNotificationWindow(const std::string& key) {
  if (items_.find(key) == items_.end()) {
    VLOG(3) << "Chrome requests to close window on notification (key: " << key
            << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->notifications(), CloseNotificationWindow);
  if (!notifications_instance)
    return;

  notifications_instance->CloseNotificationWindow(key);
}

void ArcNotificationManager::OpenNotificationSettings(const std::string& key) {
  if (items_.find(key) == items_.end()) {
    DVLOG(3) << "Chrome requests to fire a click event on the notification "
             << "settings button (key: " << key << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->notifications(), OpenNotificationSettings);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance)
    return;

  notifications_instance->OpenNotificationSettings(key);
}

bool ArcNotificationManager::IsOpeningSettingsSupported() const {
  const auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->notifications(), OpenNotificationSettings);
  return notifications_instance != nullptr;
}

void ArcNotificationManager::SendNotificationToggleExpansionOnChrome(
    const std::string& key) {
  if (items_.find(key) == items_.end()) {
    VLOG(3) << "Chrome requests to fire a click event on notification (key: "
            << key << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->notifications(), SendNotificationEventToAndroid);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "ARC Notification (key: " << key
            << ") is clicked, but the ARC channel has already gone.";
    return;
  }

  notifications_instance->SendNotificationEventToAndroid(
      key, mojom::ArcNotificationEvent::TOGGLE_EXPANSION);
}

void ArcNotificationManager::OnToastPosted(mojom::ArcToastDataPtr data) {
  const base::string16 text16(
      base::UTF8ToUTF16(data->text.has_value() ? *data->text : std::string()));
  const base::string16 dismiss_text16(base::UTF8ToUTF16(
      data->dismiss_text.has_value() ? *data->dismiss_text : std::string()));
  ash::Shell::Get()->toast_manager()->Show(
      ash::ToastData(data->id, text16, data->duration, dismiss_text16));
}

void ArcNotificationManager::OnToastCancelled(mojom::ArcToastDataPtr data) {
  ash::Shell::Get()->toast_manager()->Cancel(data->id);
}

}  // namespace arc
