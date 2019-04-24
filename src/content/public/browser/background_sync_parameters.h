// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_PARAMETERS_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_PARAMETERS_H_

#include <stdint.h>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT BackgroundSyncParameters {
  BackgroundSyncParameters();
  bool operator==(const BackgroundSyncParameters& other) const;

  // True if the manager should be disabled and registration attempts should
  // fail.
  bool disable;

  // The number of attempts the BackgroundSyncManager will make to fire an event
  // before giving up.
  int max_sync_attempts;

  // The first time that a registration retries, it will wait at least this much
  // time before doing so.
  base::TimeDelta initial_retry_delay;

  // The factor by which retry delay increases. The retry time is determined by:
  // initial_retry_delay * pow(retry_delay_factor, |attempts|-1).
  int retry_delay_factor;

  // The minimum time to wait before waking the browser in case the browser
  // closes mid-sync.
  base::TimeDelta min_sync_recovery_time;

  // The maximum amount of time that a sync event can run for.
  base::TimeDelta max_sync_event_duration;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_PARAMETERS_H_
