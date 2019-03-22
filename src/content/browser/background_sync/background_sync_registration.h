// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_H_

#include <stdint.h>

#include <list>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "content/browser/background_sync/background_sync.pb.h"
#include "content/browser/background_sync/background_sync_registration_options.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/modules/background_sync/background_sync.mojom.h"

namespace content {

class CONTENT_EXPORT BackgroundSyncRegistration {
 public:
  BackgroundSyncRegistration() = default;
  BackgroundSyncRegistration(const BackgroundSyncRegistration& other) = default;
  BackgroundSyncRegistration& operator=(
      const BackgroundSyncRegistration& other) = default;
  ~BackgroundSyncRegistration() = default;

  bool Equals(const BackgroundSyncRegistration& other) const;
  bool IsFiring() const;

  const BackgroundSyncRegistrationOptions* options() const { return &options_; }
  BackgroundSyncRegistrationOptions* options() { return &options_; }

  blink::mojom::BackgroundSyncState sync_state() const { return sync_state_; }
  void set_sync_state(blink::mojom::BackgroundSyncState state) {
    sync_state_ = state;
  }

  int num_attempts() const { return num_attempts_; }
  void set_num_attempts(int num_attempts) { num_attempts_ = num_attempts; }

  base::Time delay_until() const { return delay_until_; }
  void set_delay_until(base::Time delay_until) { delay_until_ = delay_until; }

  // By default, new registrations will not fire until set_resolved is called
  // after the registration resolves.
  bool resolved() const { return resolved_; }
  void set_resolved() { resolved_ = true; }

 private:
  BackgroundSyncRegistrationOptions options_;
  blink::mojom::BackgroundSyncState sync_state_ =
      blink::mojom::BackgroundSyncState::PENDING;
  int num_attempts_ = 0;
  base::Time delay_until_;

  // This member is not persisted to disk. It should be false until the client
  // has acknowledged tha it has resolved its registration promise.
  bool resolved_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_H_
