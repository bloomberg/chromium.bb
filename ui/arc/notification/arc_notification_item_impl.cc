// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_notification_item_impl.h"

#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/arc/notification/arc_notification_delegate.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"
#include "ui/message_center/public/cpp/message_center_constants.h"

namespace arc {

namespace {

constexpr char kNotifierId[] = "ARC_NOTIFICATION";
constexpr char kNotificationIdPrefix[] = "ARC_NOTIFICATION_";

// Converts from Android notification priority to Chrome notification priority.
// On Android, PRIORITY_DEFAULT does not pop up, so this maps PRIORITY_DEFAULT
// to Chrome's -1 to adapt that behavior. Also, this maps PRIORITY_LOW and
// _HIGH to -2 and 0 respectively to adjust the value with keeping the order
// among _LOW, _DEFAULT and _HIGH. static
// TODO(yoshiki): rewrite this conversion as typemap
int ConvertAndroidPriority(mojom::ArcNotificationPriority android_priority) {
  switch (android_priority) {
    case mojom::ArcNotificationPriority::MIN:
    case mojom::ArcNotificationPriority::LOW:
      return message_center::MIN_PRIORITY;
    case mojom::ArcNotificationPriority::DEFAULT:
      return message_center::LOW_PRIORITY;
    case mojom::ArcNotificationPriority::HIGH:
      return message_center::DEFAULT_PRIORITY;
    case mojom::ArcNotificationPriority::MAX:
      return message_center::MAX_PRIORITY;
  }

  NOTREACHED() << "Invalid Priority: " << android_priority;
  return message_center::DEFAULT_PRIORITY;
}

}  // anonymous namespace

ArcNotificationItemImpl::ArcNotificationItemImpl(
    ArcNotificationManager* manager,
    message_center::MessageCenter* message_center,
    const std::string& notification_key,
    const AccountId& profile_id)
    : manager_(manager),
      message_center_(message_center),
      profile_id_(profile_id),
      notification_key_(notification_key),
      notification_id_(kNotificationIdPrefix + notification_key_),
      weak_ptr_factory_(this) {}

ArcNotificationItemImpl::~ArcNotificationItemImpl() {
  for (auto& observer : observers_)
    observer.OnItemDestroying();
}

void ArcNotificationItemImpl::OnUpdatedFromAndroid(
    mojom::ArcNotificationDataPtr data) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(notification_key_, data->key);

  message_center::RichNotificationData rich_data;
  rich_data.pinned = (data->no_clear || data->ongoing_event);
  rich_data.priority = ConvertAndroidPriority(data->priority);
  if (data->small_icon)
    rich_data.small_image = gfx::Image::CreateFrom1xBitmap(*data->small_icon);
  if (data->accessible_name.has_value()) {
    accessible_name_ = base::UTF8ToUTF16(*data->accessible_name);
  } else {
    accessible_name_ = base::JoinString(
        {base::UTF8ToUTF16(data->title), base::UTF8ToUTF16(data->message)},
        base::ASCIIToUTF16("\n"));
  }
  rich_data.accessible_name = accessible_name_;

  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT, kNotifierId);
  notifier_id.profile_id = profile_id_.GetUserEmail();

  auto notification = base::MakeUnique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_id_,
      base::UTF8ToUTF16(data->title), base::UTF8ToUTF16(data->message),
      gfx::Image(),
      base::UTF8ToUTF16("arc"),  // display source
      GURL(),                    // empty origin url, for system component
      notifier_id, rich_data,
      new ArcNotificationDelegate(weak_ptr_factory_.GetWeakPtr()));
  notification->set_timestamp(base::Time::FromJavaTime(data->time));

  expand_state_ = data->expand_state;
  shown_contents_ = data->shown_contents;

  if (!data->snapshot_image || data->snapshot_image->isNull()) {
    snapshot_ = gfx::ImageSkia();
  } else {
    snapshot_ = gfx::ImageSkia(
        gfx::ImageSkiaRep(*data->snapshot_image, data->snapshot_image_scale));
  }

  for (auto& observer : observers_)
    observer.OnItemUpdated();

  message_center_->AddNotification(std::move(notification));
}

void ArcNotificationItemImpl::OnClosedFromAndroid() {
  being_removed_by_manager_ = true;  // Closing is initiated by the manager.
  message_center_->RemoveNotification(notification_id_, false /* by_user */);
}

void ArcNotificationItemImpl::Close(bool by_user) {
  if (being_removed_by_manager_) {
    // Closing is caused by the manager, so we don't need to nofify a close
    // event to the manager.
    return;
  }

  // Do not touch its any members afterwards, because this instance will be
  // destroyed in the following call
  manager_->SendNotificationRemovedFromChrome(notification_key_);
}

void ArcNotificationItemImpl::Click() {
  manager_->SendNotificationClickedOnChrome(notification_key_);
}

void ArcNotificationItemImpl::OpenSettings() {
  manager_->OpenNotificationSettings(notification_key_);
}

bool ArcNotificationItemImpl::IsOpeningSettingsSupported() const {
  return manager_->IsOpeningSettingsSupported();
}

void ArcNotificationItemImpl::ToggleExpansion() {
  manager_->SendNotificationToggleExpansionOnChrome(notification_key_);
}

bool ArcNotificationItemImpl::CalledOnValidThread() const {
  return thread_checker_.CalledOnValidThread();
}

void ArcNotificationItemImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ArcNotificationItemImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ArcNotificationItemImpl::IncrementWindowRefCount() {
  ++window_ref_count_;
  if (window_ref_count_ == 1)
    manager_->CreateNotificationWindow(notification_key_);
}

void ArcNotificationItemImpl::DecrementWindowRefCount() {
  DCHECK_GT(window_ref_count_, 0);
  --window_ref_count_;
  if (window_ref_count_ == 0)
    manager_->CloseNotificationWindow(notification_key_);
}

const gfx::ImageSkia& ArcNotificationItemImpl::GetSnapshot() const {
  return snapshot_;
}

mojom::ArcNotificationExpandState ArcNotificationItemImpl::GetExpandState()
    const {
  return expand_state_;
}

mojom::ArcNotificationShownContents ArcNotificationItemImpl::GetShownContents()
    const {
  return shown_contents_;
}

const std::string& ArcNotificationItemImpl::GetNotificationKey() const {
  return notification_key_;
}

const std::string& ArcNotificationItemImpl::GetNotificationId() const {
  return notification_id_;
}

const base::string16& ArcNotificationItemImpl::GetAccessibleName() const {
  return accessible_name_;
}

}  // namespace arc
