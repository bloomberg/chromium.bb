// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SCHEDULED_UPDATE_CHECKER_SCOPED_WAKE_LOCK_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SCHEDULED_UPDATE_CHECKER_SCOPED_WAKE_LOCK_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom-forward.h"

namespace policy {

// Acquires a wake lock of type |type| with reason |reason| upon construction.
// Releases the wake lock upon destruction. Movable only.
class ScopedWakeLock {
 public:
  ScopedWakeLock(device::mojom::WakeLockType type, const std::string& reason);
  ~ScopedWakeLock();

  // Movable only.
  ScopedWakeLock(ScopedWakeLock&& other);
  ScopedWakeLock& operator=(ScopedWakeLock&& other);

  // Allows tests to override how this class binds a WakeLockProvider receiver.
  using WakeLockProviderBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::WakeLockProvider>)>;
  static void OverrideWakeLockProviderBinderForTesting(
      WakeLockProviderBinder binder);

 private:
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWakeLock);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SCHEDULED_UPDATE_CHECKER_SCOPED_WAKE_LOCK_H_
