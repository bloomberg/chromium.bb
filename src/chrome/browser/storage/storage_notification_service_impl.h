// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_STORAGE_NOTIFICATION_SERVICE_IMPL_H_
#define CHROME_BROWSER_STORAGE_STORAGE_NOTIFICATION_SERVICE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/storage_notification_service.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

class StorageNotificationServiceImpl
    : public content::StorageNotificationService,
      public KeyedService {
 public:
  StorageNotificationServiceImpl();
  ~StorageNotificationServiceImpl() override;
  // Called from the UI thread, this method returns a callback that can passed
  // to any thread, and proxies calls to
  // `MaybeShowStoragePressureNotification()` back to the UI thread. It wraps a
  // weak pointer to `this`.
  StoragePressureNotificationCallback
  CreateThreadSafePressureNotificationCallback() override;
  void MaybeShowStoragePressureNotification(const blink::StorageKey) override;
  base::TimeTicks GetLastSentAtForTesting() {
    return disk_pressure_notification_last_sent_at_;
  }

 private:
  base::TimeTicks disk_pressure_notification_last_sent_at_;
  base::WeakPtrFactory<StorageNotificationServiceImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_STORAGE_STORAGE_NOTIFICATION_SERVICE_IMPL_H_
