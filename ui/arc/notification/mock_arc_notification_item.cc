// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/mock_arc_notification_item.h"

namespace arc {

namespace {

constexpr char kNotificationIdPrefix[] = "ARC_NOTIFICATION_";

}  // namespace

MockArcNotificationItem::MockArcNotificationItem(
    const std::string& notification_key)
    : notification_key_(notification_key),
      notification_id_(kNotificationIdPrefix + notification_key),
      weak_factory_(this) {}

MockArcNotificationItem::~MockArcNotificationItem() = default;

void MockArcNotificationItem::Close(bool by_user) {
  count_close_++;
}

const gfx::ImageSkia& MockArcNotificationItem::GetSnapshot() const {
  return snapshot_;
}

const std::string& MockArcNotificationItem::GetNotificationKey() const {
  return notification_key_;
}

const std::string& MockArcNotificationItem::GetNotificationId() const {
  return notification_id_;
}

bool MockArcNotificationItem::IsOpeningSettingsSupported() const {
  return true;
}

mojom::ArcNotificationType MockArcNotificationItem::GetNotificationType()
    const {
  return mojom::ArcNotificationType::SIMPLE;
}

mojom::ArcNotificationExpandState MockArcNotificationItem::GetExpandState()
    const {
  return mojom::ArcNotificationExpandState::FIXED_SIZE;
}

mojom::ArcNotificationShownContents MockArcNotificationItem::GetShownContents()
    const {
  return mojom::ArcNotificationShownContents::CONTENTS_SHOWN;
}

gfx::Rect MockArcNotificationItem::GetSwipeInputRect() const {
  return gfx::Rect();
}

const base::string16& MockArcNotificationItem::GetAccessibleName() const {
  return base::EmptyString16();
}

bool MockArcNotificationItem::IsManuallyExpandedOrCollapsed() const {
  return false;
}

}  // namespace arc
