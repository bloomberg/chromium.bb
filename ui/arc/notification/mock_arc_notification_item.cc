// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/mock_arc_notification_item.h"

#include <utility>

#include "base/bind_helpers.h"

namespace arc {

namespace {

constexpr char kNotificationIdPrefix[] = "ARC_NOTIFICATION_";

}  // namespace

MockArcNotificationItem::MockArcNotificationItem(
    const std::string& notification_key)
    : notification_key_(notification_key),
      notification_id_(kNotificationIdPrefix + notification_key),
      weak_factory_(this) {}

MockArcNotificationItem::~MockArcNotificationItem() {
  for (auto& observer : observers_)
    observer.OnItemDestroying();
}

void MockArcNotificationItem::SetCloseCallback(
    base::OnceClosure close_callback) {
  close_callback_ = std::move(close_callback);
}

void MockArcNotificationItem::Close(bool by_user) {
  count_close_++;

  if (close_callback_)
    base::ResetAndReturn(&close_callback_).Run();
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

void MockArcNotificationItem::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockArcNotificationItem::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
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

bool MockArcNotificationItem::IsManuallyExpandedOrCollapsed() const {
  return false;
}

}  // namespace arc
