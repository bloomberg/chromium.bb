// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_notification_item.h"

#include <algorithm>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"

namespace arc {

namespace {

static const char kNotifierId[] = "ARC_NOTIFICATION";

static const char kNotificationIdPrefix[] = "ARC_NOTIFICATION_";

SkBitmap DecodeImage(const std::vector<uint8_t>& data) {
  DCHECK(base::WorkerPool::RunsTasksOnCurrentThread());
  DCHECK(!data.empty());  // empty string should be handled in caller.

  // We may decode an image in the browser process since it has been generated
  // in NotificationListerService in Android and should be safe.
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(&data[0], data.size(), &bitmap);
  return bitmap;
}

class ArcNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit ArcNotificationDelegate(base::WeakPtr<ArcNotificationItem> item)
      : item_(item) {}

  void Close(bool by_user) override {
    if (item_)
      item_->Close(by_user);
  }

  // Indicates all notifications have a click handler. This changes the mouse
  // cursor on hover.
  // TODO(yoshiki): Return the correct value according to the content intent
  // and the flags.
  bool HasClickedListener() override { return true; }

  void Click() override {
    if (item_)
      item_->Click();
  }

 private:
  // The destructor is private since this class is ref-counted.
  ~ArcNotificationDelegate() override {}

  base::WeakPtr<ArcNotificationItem> item_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationDelegate);
};

}  // anonymous namespace

ArcNotificationItem::ArcNotificationItem(
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

void ArcNotificationItem::UpdateWithArcNotificationData(
    const ArcNotificationData& data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(notification_key_ == data.key);

  // Check if a decode task is on-going or not. If |notification_| is non-null,
  // a decode task is on-going asynchronously. Otherwise, there is no task.
  // TODO(yoshiki): Refactor and remove this check by omitting image decoding
  // from here.
  if (notification_) {
    // Store the latest data to the |newer_data_| property and returns, if the
    // previous decode is still in progress.
    // If old |newer_data_| has been stored, discard the old one.
    newer_data_ = data.Clone();
    return;
  }

  message_center::RichNotificationData rich_data;
  message_center::NotificationType type;

  switch (data.type) {
    case ARC_NOTIFICATION_TYPE_BASIC:
      type = message_center::NOTIFICATION_TYPE_SIMPLE;
      break;
    case ARC_NOTIFICATION_TYPE_IMAGE:
      // TODO(yoshiki): Implement this types.
      type = message_center::NOTIFICATION_TYPE_SIMPLE;
      LOG(ERROR) << "Unsupported notification type: image";
      break;
    case ARC_NOTIFICATION_TYPE_PROGRESS:
      type = message_center::NOTIFICATION_TYPE_PROGRESS;
      rich_data.timestamp = base::Time::UnixEpoch() +
                            base::TimeDelta::FromMilliseconds(data.time);
      rich_data.progress = std::max(
          0, std::min(100, static_cast<int>(std::round(
                               static_cast<float>(data.progress_current) /
                               data.progress_max * 100))));
      break;
  }
  DCHECK(0 <= data.type && data.type <= ARC_NOTIFICATION_TYPE_MAX)
      << "Unsupported notification type: " << data.type;

  // The identifier of the notifier, which is used to distinguish the notifiers
  // in the message center.
  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT, kNotifierId);
  notifier_id.profile_id = profile_id_.GetUserEmail();

  DCHECK(!data.title.is_null());
  DCHECK(!data.message.is_null());
  notification_.reset(new message_center::Notification(
      type, notification_id_, base::UTF8ToUTF16(data.title.get()),
      base::UTF8ToUTF16(data.message.get()),
      gfx::Image(),              // icon image: Will be overriden later.
      base::UTF8ToUTF16("arc"),  // display source
      GURL(),                    // empty origin url, for system component
      notifier_id, rich_data,
      new ArcNotificationDelegate(weak_ptr_factory_.GetWeakPtr())));

  DCHECK(!data.icon_data.is_null());
  if (data.icon_data.size() == 0) {
    OnImageDecoded(SkBitmap());  // Pass an empty bitmap.
    return;
  }

  // TODO(yoshiki): Remove decoding by passing a bitmap directly from Android.
  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true).get(), FROM_HERE,
      base::Bind(&DecodeImage, data.icon_data.storage()),
      base::Bind(&ArcNotificationItem::OnImageDecoded,
                 weak_ptr_factory_.GetWeakPtr()));
}

ArcNotificationItem::~ArcNotificationItem() {}

void ArcNotificationItem::OnClosedFromAndroid() {
  being_removed_by_manager_ = true;  // Closing is initiated by the manager.
  message_center_->RemoveNotification(notification_id_, true /* by_user */);
}

void ArcNotificationItem::Close(bool by_user) {
  if (being_removed_by_manager_) {
    // Closing is caused by the manager, so we don't need to nofify a close
    // event to the manager.
    return;
  }

  // Do not touch its any members afterwards, because this instance will be
  // destroyed in the following call
  manager_->SendNotificationRemovedFromChrome(notification_key_);
}

void ArcNotificationItem::Click() {
  manager_->SendNotificationClickedOnChrome(notification_key_);
}

void ArcNotificationItem::OnImageDecoded(const SkBitmap& bitmap) {
  DCHECK(thread_checker_.CalledOnValidThread());

  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);
  notification_->set_icon(image);

  DCHECK(notification_);
  message_center_->AddNotification(std::move(notification_));

  if (newer_data_) {
    // There is the newer data, so updates again.
    ArcNotificationDataPtr data(std::move(newer_data_));
    UpdateWithArcNotificationData(*data);
  }
}

}  // namespace arc
