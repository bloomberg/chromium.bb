// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/p2p_notifier.h"

#include <algorithm>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/values.h"
#include "sync/notifier/sync_notifier_observer.h"
#include "sync/syncable/model_type_payload_map.h"

namespace sync_notifier {

const char* kSyncP2PNotificationChannel = "http://www.google.com/chrome/sync";

namespace {

const char kNotifySelf[] = "notifySelf";
const char kNotifyOthers[] = "notifyOthers";
const char kNotifyAll[] = "notifyAll";

const char kSenderIdKey[] = "senderId";
const char kNotificationTypeKey[] = "notificationType";
const char kChangedTypesKey[] = "changedTypes";

}  // namespace

std::string P2PNotificationTargetToString(P2PNotificationTarget target) {
  switch (target) {
    case NOTIFY_SELF:
      return kNotifySelf;
    case NOTIFY_OTHERS:
      return kNotifyOthers;
    case NOTIFY_ALL:
      return kNotifyAll;
    default:
      NOTREACHED();
      return "";
  }
}

P2PNotificationTarget P2PNotificationTargetFromString(
    const std::string& target_str) {
  if (target_str == kNotifySelf) {
    return NOTIFY_SELF;
  }
  if (target_str == kNotifyOthers) {
    return NOTIFY_OTHERS;
  }
  if (target_str == kNotifyAll) {
    return NOTIFY_ALL;
  }
  LOG(WARNING) << "Could not parse " << target_str;
  return NOTIFY_SELF;
}

P2PNotificationData::P2PNotificationData() : target_(NOTIFY_SELF) {}

P2PNotificationData::P2PNotificationData(
    const std::string& sender_id,
    P2PNotificationTarget target,
    syncable::ModelTypeSet changed_types)
    : sender_id_(sender_id),
      target_(target),
      changed_types_(changed_types) {}

P2PNotificationData::~P2PNotificationData() {}

bool P2PNotificationData::IsTargeted(const std::string& id) const {
  switch (target_) {
    case NOTIFY_SELF:
      return sender_id_ == id;
    case NOTIFY_OTHERS:
      return sender_id_ != id;
    case NOTIFY_ALL:
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

syncable::ModelTypeSet P2PNotificationData::GetChangedTypes() const {
  return changed_types_;
}

bool P2PNotificationData::Equals(const P2PNotificationData& other) const {
  return
      (sender_id_ == other.sender_id_) &&
      (target_ == other.target_) &&
      changed_types_.Equals(other.changed_types_);
}

std::string P2PNotificationData::ToString() const {
  scoped_ptr<DictionaryValue> dict(new DictionaryValue());
  dict->SetString(kSenderIdKey, sender_id_);
  dict->SetString(kNotificationTypeKey,
                  P2PNotificationTargetToString(target_));
  dict->Set(kChangedTypesKey, syncable::ModelTypeSetToValue(changed_types_));
  std::string json;
  base::JSONWriter::Write(dict.get(), &json);
  return json;
}

bool P2PNotificationData::ResetFromString(const std::string& str) {
  scoped_ptr<Value> data_value(base::JSONReader::Read(str));
  if (!data_value.get()) {
    LOG(WARNING) << "Could not parse " << str;
    return false;
  }
  if (!data_value->IsType(Value::TYPE_DICTIONARY)) {
    LOG(WARNING) << "Could not parse " << str << " as a dictionary";
    return false;
  }
  // TODO(akalin): Use Values::AsDictionary() when it becomes
  // available.
  DictionaryValue* data_dict =
      static_cast<DictionaryValue*>(data_value.get());
  if (!data_dict->GetString(kSenderIdKey, &sender_id_)) {
    LOG(WARNING) << "Could not find string value for " << kSenderIdKey;
  }
  std::string target_str;
  if (!data_dict->GetString(kNotificationTypeKey, &target_str)) {
    LOG(WARNING) << "Could not find string value for "
                 << kNotificationTypeKey;
  }
  target_ = P2PNotificationTargetFromString(target_str);
  ListValue* changed_types_list = NULL;
  if (!data_dict->GetList(kChangedTypesKey, &changed_types_list)) {
    LOG(WARNING) << "Could not find list value for "
                 << kChangedTypesKey;
    return false;
  }
  changed_types_ = syncable::ModelTypeSetFromValue(*changed_types_list);
  return true;
}

P2PNotifier::P2PNotifier(const notifier::NotifierOptions& notifier_options,
                         P2PNotificationTarget send_notification_target)
    : push_client_(notifier_options),
      logged_in_(false),
      notifications_enabled_(false),
      send_notification_target_(send_notification_target),
      parent_message_loop_proxy_(base::MessageLoopProxy::current()) {
  DCHECK(send_notification_target_ == NOTIFY_OTHERS ||
         send_notification_target_ == NOTIFY_ALL);
  push_client_.AddObserver(this);
}

P2PNotifier::~P2PNotifier() {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  push_client_.RemoveObserver(this);
}

void P2PNotifier::AddObserver(SyncNotifierObserver* observer) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  observer_list_.AddObserver(observer);
}

void P2PNotifier::RemoveObserver(SyncNotifierObserver* observer) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  observer_list_.RemoveObserver(observer);
}

void P2PNotifier::SetUniqueId(const std::string& unique_id) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  unique_id_ = unique_id;
}

void P2PNotifier::SetState(const std::string& state) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
}

void P2PNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  notifier::Subscription subscription;
  subscription.channel = kSyncP2PNotificationChannel;
  // There may be some subtle issues around case sensitivity of the
  // from field, but it doesn't matter too much since this is only
  // used in p2p mode (which is only used in testing).
  subscription.from = email;
  push_client_.UpdateSubscriptions(
      notifier::SubscriptionList(1, subscription));

  // If already logged in, the new credentials will take effect on the
  // next reconnection.
  push_client_.UpdateCredentials(email, token);
  logged_in_ = true;
}

void P2PNotifier::UpdateEnabledTypes(
    syncable::ModelTypeSet enabled_types) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  const syncable::ModelTypeSet new_enabled_types =
      Difference(enabled_types, enabled_types_);
  enabled_types_ = enabled_types;
  const P2PNotificationData notification_data(
      unique_id_, NOTIFY_SELF, new_enabled_types);
  SendNotificationData(notification_data);
}

void P2PNotifier::SendNotification(
    syncable::ModelTypeSet changed_types) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  const P2PNotificationData notification_data(
      unique_id_, send_notification_target_, changed_types);
  SendNotificationData(notification_data);
}

void P2PNotifier::OnNotificationStateChange(bool notifications_enabled) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  bool disabled_to_enabled = notifications_enabled && !notifications_enabled_;
  notifications_enabled_ = notifications_enabled;
  FOR_EACH_OBSERVER(SyncNotifierObserver, observer_list_,
      OnNotificationStateChange(notifications_enabled_));
  if (disabled_to_enabled) {
    const P2PNotificationData notification_data(
        unique_id_, NOTIFY_SELF, enabled_types_);
    SendNotificationData(notification_data);
  }
}

void P2PNotifier::OnIncomingNotification(
    const notifier::Notification& notification) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  DVLOG(1) << "Received notification " << notification.ToString();
  if (!logged_in_) {
    DVLOG(1) << "Not logged in yet -- not emitting notification";
    return;
  }
  if (!notifications_enabled_) {
    DVLOG(1) << "Notifications not enabled -- not emitting notification";
    return;
  }
  if (notification.channel != kSyncP2PNotificationChannel) {
    LOG(WARNING) << "Notification from unexpected source "
                 << notification.channel;
  }
  P2PNotificationData notification_data;
  if (!notification_data.ResetFromString(notification.data)) {
    LOG(WARNING) << "Could not parse notification data from "
                 << notification.data;
    notification_data =
        P2PNotificationData(unique_id_, NOTIFY_ALL, enabled_types_);
  }
  if (!notification_data.IsTargeted(unique_id_)) {
    DVLOG(1) << "Not a target of the notification -- "
             << "not emitting notification";
    return;
  }
  if (notification_data.GetChangedTypes().Empty()) {
    DVLOG(1) << "No changed types -- not emitting notification";
    return;
  }
  const syncable::ModelTypePayloadMap& type_payloads =
      syncable::ModelTypePayloadMapFromEnumSet(
          notification_data.GetChangedTypes(), std::string());
  FOR_EACH_OBSERVER(SyncNotifierObserver, observer_list_,
                    OnIncomingNotification(type_payloads, REMOTE_NOTIFICATION));
}

void P2PNotifier::SimulateConnectForTest(
    base::WeakPtr<buzz::XmppTaskParentInterface> base_task) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  push_client_.SimulateConnectAndSubscribeForTest(base_task);
}

void P2PNotifier::ReflectSentNotificationsForTest() {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  push_client_.ReflectSentNotificationsForTest();
}

void P2PNotifier::SendNotificationDataForTest(
    const P2PNotificationData& notification_data) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  SendNotificationData(notification_data);
}

void P2PNotifier::SendNotificationData(
    const P2PNotificationData& notification_data) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  notifier::Notification notification;
  notification.channel = kSyncP2PNotificationChannel;
  notification.data = notification_data.ToString();
  DVLOG(1) << "Sending XMPP notification: " << notification.ToString();
  push_client_.SendNotification(notification);
}

}  // namespace sync_notifier
