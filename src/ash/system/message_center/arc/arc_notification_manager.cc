// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/arc/arc_notification_manager.h"

#include <memory>
#include <utility>

#include "ash/system/message_center/arc/arc_notification_delegate.h"
#include "ash/system/message_center/arc/arc_notification_item_impl.h"
#include "ash/system/message_center/arc/arc_notification_manager_delegate.h"
#include "ash/system/message_center/arc/arc_notification_view.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/mojo_channel.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/views/message_view_factory.h"

using arc::ConnectionHolder;
using arc::MojoChannel;
using arc::mojom::ArcNotificationData;
using arc::mojom::ArcNotificationDataPtr;
using arc::mojom::ArcNotificationEvent;
using arc::mojom::ArcNotificationPriority;
using arc::mojom::ArcDoNotDisturbStatus;
using arc::mojom::ArcDoNotDisturbStatusPtr;
using arc::mojom::NotificationsHost;
using arc::mojom::NotificationsInstance;
using arc::mojom::NotificationsInstancePtr;

namespace ash {
namespace {

constexpr char kPlayStorePackageName[] = "com.android.vending";

std::unique_ptr<message_center::MessageView> CreateCustomMessageView(
    const message_center::Notification& notification) {
  DCHECK_EQ(notification.notifier_id().type,
            message_center::NotifierId::ARC_APPLICATION);
  auto* arc_delegate =
      static_cast<ArcNotificationDelegate*>(notification.delegate());
  return arc_delegate->CreateCustomMessageView(notification);
}

class DoNotDisturbManager : public message_center::MessageCenterObserver {
 public:
  DoNotDisturbManager(ArcNotificationManager* manager) : manager_(manager) {}
  void OnQuietModeChanged(bool in_quiet_mode) override {
    manager_->SetDoNotDisturbStatusOnAndroid(in_quiet_mode);
  }

 private:
  ArcNotificationManager* const manager_;

  DISALLOW_COPY_AND_ASSIGN(DoNotDisturbManager);
};

}  // namespace

class ArcNotificationManager::InstanceOwner {
 public:
  InstanceOwner() = default;
  ~InstanceOwner() = default;

  void SetInstancePtr(NotificationsInstancePtr instance_ptr) {
    DCHECK(!channel_);

    channel_ =
        std::make_unique<MojoChannel<NotificationsInstance, NotificationsHost>>(
            &holder_, std::move(instance_ptr));

    // Using base::Unretained because |this| owns |channel_|.
    channel_->set_connection_error_handler(
        base::BindOnce(&InstanceOwner::OnDisconnected, base::Unretained(this)));
    channel_->QueryVersion();
  }

  ConnectionHolder<NotificationsInstance, NotificationsHost>* holder() {
    return &holder_;
  }

 private:
  void OnDisconnected() { channel_.reset(); }

  ConnectionHolder<NotificationsInstance, NotificationsHost> holder_;
  std::unique_ptr<MojoChannel<NotificationsInstance, NotificationsHost>>
      channel_;

  DISALLOW_COPY_AND_ASSIGN(InstanceOwner);
};

// static
void ArcNotificationManager::SetCustomNotificationViewFactory() {
  message_center::MessageViewFactory::SetCustomNotificationViewFactory(
      base::BindRepeating(&CreateCustomMessageView));
}

ArcNotificationManager::ArcNotificationManager(
    std::unique_ptr<ArcNotificationManagerDelegate> delegate,
    const AccountId& main_profile_id,
    message_center::MessageCenter* message_center)
    : delegate_(std::move(delegate)),
      main_profile_id_(main_profile_id),
      message_center_(message_center),
      do_not_disturb_manager_(new DoNotDisturbManager(this)),
      instance_owner_(std::make_unique<InstanceOwner>()) {
  DCHECK(message_center_);

  instance_owner_->holder()->SetHost(this);
  instance_owner_->holder()->AddObserver(this);
  if (!message_center::MessageViewFactory::HasCustomNotificationViewFactory())
    SetCustomNotificationViewFactory();

  message_center_->AddObserver(do_not_disturb_manager_.get());
}

ArcNotificationManager::~ArcNotificationManager() {
  message_center_->RemoveObserver(do_not_disturb_manager_.get());

  instance_owner_->holder()->RemoveObserver(this);
  instance_owner_->holder()->SetHost(nullptr);

  // Ensures that any callback tied to |instance_owner_| is not invoked.
  instance_owner_.reset();
}

void ArcNotificationManager::SetInstance(NotificationsInstancePtr instance) {
  instance_owner_->SetInstancePtr(std::move(instance));
}

ConnectionHolder<NotificationsInstance, NotificationsHost>*
ArcNotificationManager::GetConnectionHolderForTest() {
  return instance_owner_->holder();
}

void ArcNotificationManager::OnConnectionReady() {
  DCHECK(!ready_);

  // TODO(hidehiko): Replace this by ConnectionHolder::IsConnected().
  ready_ = true;

  // Sync the initial quiet mode state with Android.
  SetDoNotDisturbStatusOnAndroid(message_center_->IsQuietMode());
}

void ArcNotificationManager::OnConnectionClosed() {
  DCHECK(ready_);
  while (!items_.empty()) {
    auto it = items_.begin();
    std::unique_ptr<ArcNotificationItem> item = std::move(it->second);
    items_.erase(it);
    item->OnClosedFromAndroid();
  }
  ready_ = false;
}

void ArcNotificationManager::OnNotificationPosted(ArcNotificationDataPtr data) {
  if (ShouldIgnoreNotification(data.get())) {
    VLOG(3) << "Posted notification was ignored.";
    return;
  }

  const std::string& key = data->key;
  auto it = items_.find(key);
  if (it == items_.end()) {
    // Show a notification on the primary logged-in user's desktop and badge the
    // app icon in the shelf if the icon exists.
    // TODO(yoshiki): Reconsider when ARC supports multi-user.
    auto item = std::make_unique<ArcNotificationItemImpl>(
        this, message_center_, key, main_profile_id_);
    // TODO(yoshiki): Use emplacement for performance when it's available.
    auto result = items_.insert(std::make_pair(key, std::move(item)));
    DCHECK(result.second);
    it = result.first;
  }

  delegate_->GetAppIdByPackageName(
      data->package_name.value_or(std::string()),
      base::BindOnce(&ArcNotificationManager::OnGotAppId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(data)));
}

void ArcNotificationManager::OnNotificationUpdated(
    ArcNotificationDataPtr data) {
  if (ShouldIgnoreNotification(data.get())) {
    VLOG(3) << "Updated notification was ignored.";
    return;
  }

  const std::string& key = data->key;
  auto it = items_.find(key);
  if (it == items_.end())
    return;

  bool is_remote_view_focused =
      (data->remote_input_state ==
       arc::mojom::ArcNotificationRemoteInputState::OPENED);
  if (is_remote_view_focused && (previously_focused_notification_key_ != key)) {
    if (!previously_focused_notification_key_.empty()) {
      auto prev_it = items_.find(previously_focused_notification_key_);
      // The case that another remote input is focused. Notify the previously-
      // focused notification (if any).
      if (prev_it != items_.end())
        prev_it->second->OnRemoteInputActivationChanged(false);
    }

    // Notify the newly-focused notification.
    previously_focused_notification_key_ = key;
    it->second->OnRemoteInputActivationChanged(true);
  } else if (!is_remote_view_focused &&
             (previously_focused_notification_key_ == key)) {
    // The case that the previously-focused notification gets unfocused. Notify
    // the previously-focused notification if the notification still exists.
    auto it = items_.find(previously_focused_notification_key_);
    if (it != items_.end())
      it->second->OnRemoteInputActivationChanged(false);

    previously_focused_notification_key_.clear();
  }

  delegate_->GetAppIdByPackageName(
      data->package_name.value_or(std::string()),
      base::BindOnce(&ArcNotificationManager::OnGotAppId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(data)));
}

void ArcNotificationManager::OpenMessageCenter() {
  delegate_->ShowMessageCenter();
}

void ArcNotificationManager::CloseMessageCenter() {
  delegate_->HideMessageCenter();
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
      instance_owner_->holder(), SendNotificationEventToAndroid);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "ARC Notification (key: " << key
            << ") is closed, but the ARC channel has already gone.";
    return;
  }

  notifications_instance->SendNotificationEventToAndroid(
      key, ArcNotificationEvent::CLOSED);
}

void ArcNotificationManager::SendNotificationClickedOnChrome(
    const std::string& key) {
  if (items_.find(key) == items_.end()) {
    VLOG(3) << "Chrome requests to fire a click event on notification (key: "
            << key << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), SendNotificationEventToAndroid);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "ARC Notification (key: " << key
            << ") is clicked, but the ARC channel has already gone.";
    return;
  }

  notifications_instance->SendNotificationEventToAndroid(
      key, ArcNotificationEvent::BODY_CLICKED);
}

void ArcNotificationManager::CreateNotificationWindow(const std::string& key) {
  if (items_.find(key) == items_.end()) {
    VLOG(3) << "Chrome requests to create window on notification (key: " << key
            << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), CreateNotificationWindow);
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
      instance_owner_->holder(), CloseNotificationWindow);
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
      instance_owner_->holder(), OpenNotificationSettings);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance)
    return;

  notifications_instance->OpenNotificationSettings(key);
}

void ArcNotificationManager::OpenNotificationSnoozeSettings(
    const std::string& key) {
  if (!base::ContainsKey(items_, key)) {
    DVLOG(3) << "Chrome requests to show a snooze setting gut on the"
             << "notification (key: " << key << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), OpenNotificationSnoozeSettings);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance)
    return;

  notifications_instance->OpenNotificationSnoozeSettings(key);
}

bool ArcNotificationManager::IsOpeningSettingsSupported() const {
  const auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), OpenNotificationSettings);
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
      instance_owner_->holder(), SendNotificationEventToAndroid);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "ARC Notification (key: " << key
            << ") is clicked, but the ARC channel has already gone.";
    return;
  }

  notifications_instance->SendNotificationEventToAndroid(
      key, ArcNotificationEvent::TOGGLE_EXPANSION);
}

bool ArcNotificationManager::ShouldIgnoreNotification(
    ArcNotificationData* data) {
  if (data->priority == ArcNotificationPriority::NONE)
    return true;

  // Notifications from Play Store are ignored in Public Session and Kiosk mode.
  // TODO: Use centralized const for Play Store package.
  if (data->package_name.has_value() &&
      *data->package_name == kPlayStorePackageName &&
      delegate_->IsPublicSessionOrKiosk()) {
    return true;
  }

  return false;
}

void ArcNotificationManager::OnGotAppId(ArcNotificationDataPtr data,
                                        const std::string& app_id) {
  const std::string& key = data->key;
  auto it = items_.find(key);
  if (it == items_.end())
    return;

  it->second->OnUpdatedFromAndroid(std::move(data), app_id);
}

void ArcNotificationManager::OnDoNotDisturbStatusUpdated(
    ArcDoNotDisturbStatusPtr status) {
  // Remove the observer to prevent from sending the command to Android since
  // this request came from Android.
  message_center_->RemoveObserver(do_not_disturb_manager_.get());

  if (message_center_->IsQuietMode() != status->enabled)
    message_center_->SetQuietMode(status->enabled);

  // Add back the observer.
  message_center_->AddObserver(do_not_disturb_manager_.get());
}

void ArcNotificationManager::SetDoNotDisturbStatusOnAndroid(bool enabled) {
  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), SetDoNotDisturbStatusOnAndroid);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "Trying to send the Do Not Disturb status (" << enabled
            << "), but the ARC channel has already gone.";
    return;
  }

  ArcDoNotDisturbStatusPtr status = ArcDoNotDisturbStatus::New();
  status->enabled = enabled;

  notifications_instance->SetDoNotDisturbStatusOnAndroid(std::move(status));
}

void ArcNotificationManager::CancelLongPress(const std::string& key) {
  auto* notifications_instance =
      ARC_GET_INSTANCE_FOR_METHOD(instance_owner_->holder(), CancelLongPress);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "Trying to cancel the long press operation, "
               "but the ARC channel has already gone.";
    return;
  }

  notifications_instance->CancelLongPress(key);
}

}  // namespace ash
