// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_MOCK_ARC_NOTIFICATION_ITEM_H_
#define UI_ARC_NOTIFICATION_MOCK_ARC_NOTIFICATION_ITEM_H_

#include <string>

#include "ui/arc/notification/arc_notification_item.h"

namespace arc {

class MockArcNotificationItem : public ArcNotificationItem {
 public:
  MockArcNotificationItem(const std::string& notification_key);
  ~MockArcNotificationItem() override;

  // Methods for testing.
  size_t count_close() { return count_close_; }
  base::WeakPtr<MockArcNotificationItem> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Overriding methods for testing.
  void Close(bool by_user) override;
  const gfx::ImageSkia& GetSnapshot() const override;
  const std::string& GetNotificationKey() const override;
  const std::string& GetNotificationId() const override;

  // Overriding methods for returning dummy data or doing nothing.
  void OnClosedFromAndroid() override {}
  void Click() override {}
  void ToggleExpansion() override {}
  void OpenSettings() override {}
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  void IncrementWindowRefCount() override {}
  void DecrementWindowRefCount() override {}
  bool IsOpeningSettingsSupported() const override;
  mojom::ArcNotificationType GetNotificationType() const override;
  mojom::ArcNotificationExpandState GetExpandState() const override;
  mojom::ArcNotificationShownContents GetShownContents() const override;
  gfx::Rect GetSwipeInputRect() const override;
  const base::string16& GetAccessibleName() const override;

  void OnUpdatedFromAndroid(mojom::ArcNotificationDataPtr data,
                            const std::string& app_id) override {}
  bool IsManuallyExpandedOrCollapsed() const override;

 private:
  std::string notification_key_;
  std::string notification_id_;
  gfx::ImageSkia snapshot_;
  size_t count_close_ = 0;

  base::WeakPtrFactory<MockArcNotificationItem> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockArcNotificationItem);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_MOCK_ARC_NOTIFICATION_ITEM_H_
