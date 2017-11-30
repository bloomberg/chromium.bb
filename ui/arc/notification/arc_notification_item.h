// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_ITEM_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_ITEM_H_

#include "base/macros.h"
#include "components/arc/common/notifications.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace arc {

class ArcNotificationItem {
 public:
  class Observer {
   public:
    // Invoked when the notification data for this item has changed.
    virtual void OnItemDestroying() = 0;

    // Invoked when the notification data for the item is updated.
    virtual void OnItemUpdated() = 0;

   protected:
    virtual ~Observer() = default;
  };

  virtual ~ArcNotificationItem() = default;

  // Called when the notification is closed on Android-side. This is called from
  // ArcNotificationManager.
  virtual void OnClosedFromAndroid() = 0;
  // Called when the notification is updated on Android-side. This is called
  // from ArcNotificationManager.
  virtual void OnUpdatedFromAndroid(mojom::ArcNotificationDataPtr data) = 0;

  // Called when the notification is closed on Chrome-side. This is called from
  // ArcNotificationDelegate.
  virtual void Close(bool by_user) = 0;
  // Called when the notification is clicked by user. This is called from
  // ArcNotificationDelegate.
  virtual void Click() = 0;

  // Called when the user wants to open an intrinsic setting of notification.
  // This is called from ArcNotificationContentView.
  virtual void OpenSettings() = 0;
  // Called when the user wants to toggle expansio of notification. This is
  // called from ArcNotificationContentView.
  virtual void ToggleExpansion() = 0;
  // Returns true if this notification has an intrinsic setting which shown
  // inside the notification content area. This is called from
  // ArcNotificationContentView.
  virtual bool IsOpeningSettingsSupported() const = 0;

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;
  // Removes the observer.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Increments |window_ref_count_| and a CreateNotificationWindow request
  // is sent when |window_ref_count_| goes from zero to one.
  virtual void IncrementWindowRefCount() = 0;

  // Decrements |window_ref_count_| and a CloseNotificationWindow request
  // is sent when |window_ref_count_| goes from one to zero.
  virtual void DecrementWindowRefCount() = 0;

  // Returns the current snapshot.
  virtual const gfx::ImageSkia& GetSnapshot() const = 0;
  // Returns the current expand state.
  virtual mojom::ArcNotificationExpandState GetExpandState() const = 0;
  // Returns the current type of shown contents.
  virtual mojom::ArcNotificationShownContents GetShownContents() const = 0;
  // Returns the rect for which Android wants to handle all swipe events.
  // Defaults to the empty rectangle.
  virtual gfx::Rect GetSwipeInputRect() const = 0;
  // Returns the notification key passed from Android-side.
  virtual const std::string& GetNotificationKey() const = 0;
  // Returns the notification ID used in the Chrome message center.
  virtual const std::string& GetNotificationId() const = 0;
  // Returnes the accessible name of the notification.
  virtual const base::string16& GetAccessibleName() const = 0;
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_ITEM_H_
