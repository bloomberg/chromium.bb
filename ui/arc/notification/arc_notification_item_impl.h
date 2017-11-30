// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_ITEM_IMPL_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_ITEM_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/signin/core/account_id/account_id.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/arc/notification/arc_notification_item.h"
#include "ui/arc/notification/arc_notification_manager.h"
#include "ui/message_center/message_center.h"

namespace arc {

// The class represents each ARC notification. One instance of this class
// corresponds to one ARC notification.
class ArcNotificationItemImpl : public ArcNotificationItem {
 public:
  ArcNotificationItemImpl(ArcNotificationManager* manager,
                          message_center::MessageCenter* message_center,
                          const std::string& notification_key,
                          const AccountId& profile_id);
  ~ArcNotificationItemImpl() override;

  // ArcNotificationItem overrides:
  void OnClosedFromAndroid() override;
  void OnUpdatedFromAndroid(mojom::ArcNotificationDataPtr data) override;
  void Close(bool by_user) override;
  void Click() override;
  void OpenSettings() override;
  bool IsOpeningSettingsSupported() const override;
  void ToggleExpansion() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void IncrementWindowRefCount() override;
  void DecrementWindowRefCount() override;
  const gfx::ImageSkia& GetSnapshot() const override;
  mojom::ArcNotificationExpandState GetExpandState() const override;
  mojom::ArcNotificationShownContents GetShownContents() const override;
  gfx::Rect GetSwipeInputRect() const override;
  const std::string& GetNotificationKey() const override;
  const std::string& GetNotificationId() const override;
  const base::string16& GetAccessibleName() const override;

 private:
  // Return true if it's on the thread this instance is created on.
  bool CalledOnValidThread() const;

  ArcNotificationManager* const manager_;
  message_center::MessageCenter* const message_center_;

  // The snapshot of the latest notification.
  gfx::ImageSkia snapshot_;
  // The expand state of the latest notification.
  mojom::ArcNotificationExpandState expand_state_ =
      mojom::ArcNotificationExpandState::FIXED_SIZE;
  // The type of shown content of the latest notification.
  mojom::ArcNotificationShownContents shown_contents_ =
      mojom::ArcNotificationShownContents::CONTENTS_SHOWN;
  // Rect indicating where Android wants to handle swipe events by itself.
  gfx::Rect swipe_input_rect_ = gfx::Rect();
  // The reference counter of the window.
  int window_ref_count_ = 0;
  // The accessible name of the latest notification.
  base::string16 accessible_name_;

  base::ObserverList<Observer> observers_;

  const AccountId profile_id_;
  const std::string notification_key_;
  const std::string notification_id_;

  // The flag to indicate that the removing is initiated by the manager and we
  // don't need to notify a remove event to the manager.
  // This is true only when:
  //   (1) the notification is being removed
  //   (2) the removing is initiated by manager
  bool being_removed_by_manager_ = false;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<ArcNotificationItemImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationItemImpl);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_ITEM_IMPL_H_
