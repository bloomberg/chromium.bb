// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_custom_notification_item.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/notification_surface.h"
#include "ui/arc/notification/arc_custom_notification_view.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"

namespace arc {

namespace {

constexpr char kNotifierId[] = "ARC_NOTIFICATION";

class ArcNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit ArcNotificationDelegate(ArcCustomNotificationItem* item)
      : item_(item) {}

  std::unique_ptr<views::View> CreateCustomContent() override {
    if (!surface_)
      return nullptr;

    return base::MakeUnique<ArcCustomNotificationView>(item_, surface_);
  }

  void set_notification_surface(exo::NotificationSurface* surface) {
    surface_ = surface;
  }

 private:
  // The destructor is private since this class is ref-counted.
  ~ArcNotificationDelegate() override {}

  ArcCustomNotificationItem* const item_;
  exo::NotificationSurface* surface_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationDelegate);
};

}  // namespace

ArcCustomNotificationItem::ArcCustomNotificationItem(
    ArcNotificationManager* manager,
    message_center::MessageCenter* message_center,
    const std::string& notification_key,
    const AccountId& profile_id)
    : ArcNotificationItem(manager,
                          message_center,
                          notification_key,
                          profile_id) {
  ArcNotificationSurfaceManager::Get()->AddObserver(this);
}

ArcCustomNotificationItem::~ArcCustomNotificationItem() {
  if (ArcNotificationSurfaceManager::Get())
    ArcNotificationSurfaceManager::Get()->RemoveObserver(this);

  FOR_EACH_OBSERVER(Observer, observers_, OnItemDestroying());
}

void ArcCustomNotificationItem::UpdateWithArcNotificationData(
    const mojom::ArcNotificationData& data) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(notification_key(), data.key);

  if (CacheArcNotificationData(data))
    return;

  message_center::RichNotificationData rich_data;
  rich_data.pinned = (data.no_clear || data.ongoing_event);
  rich_data.priority = ConvertAndroidPriority(data.priority);

  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT, kNotifierId);
  notifier_id.profile_id = profile_id().GetUserEmail();

  DCHECK(!data.title.is_null());
  DCHECK(!data.message.is_null());
  SetNotification(base::MakeUnique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_id(),
      base::UTF8ToUTF16(data.title.get()),
      base::UTF8ToUTF16(data.message.get()), gfx::Image(),
      base::UTF8ToUTF16("arc"),  // display source
      GURL(),                    // empty origin url, for system component
      notifier_id, rich_data, new ArcNotificationDelegate(this)));

  exo::NotificationSurface* surface =
      ArcNotificationSurfaceManager::Get()->GetSurface(notification_key());
  if (surface)
    OnNotificationSurfaceAdded(surface);

  pinned_ = rich_data.pinned;
  FOR_EACH_OBSERVER(Observer, observers_, OnItemPinnedChanged());
}

void ArcCustomNotificationItem::CloseFromCloseButton() {
  // Needs to manually remove notification from MessageCenter because
  // the floating close button is not part of MessageCenter.
  message_center()->RemoveNotification(notification_id(), true);
  Close(true);
}

void ArcCustomNotificationItem::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ArcCustomNotificationItem::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ArcCustomNotificationItem::OnNotificationSurfaceAdded(
    exo::NotificationSurface* surface) {
  if (!pending_notification() ||
      surface->notification_id() != notification_key()) {
    return;
  }

  static_cast<ArcNotificationDelegate*>(pending_notification()->delegate())
      ->set_notification_surface(surface);
  AddToMessageCenter();
}

void ArcCustomNotificationItem::OnNotificationSurfaceRemoved(
    exo::NotificationSurface* surface) {
  if (surface->notification_id() != notification_key())
    return;

  FOR_EACH_OBSERVER(Observer, observers_, OnItemNotificationSurfaceRemoved());
}

}  // namespace arc
