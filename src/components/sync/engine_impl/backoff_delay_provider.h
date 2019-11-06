// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_BACKOFF_DELAY_PROVIDER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_BACKOFF_DELAY_PROVIDER_H_

#include "base/macros.h"
#include "base/time/time.h"

namespace syncer {

struct ModelNeutralState;

// A component used to get time delays associated with exponential backoff.
class BackoffDelayProvider {
 public:
  // Factory function to create a standard BackoffDelayProvider.
  static BackoffDelayProvider* FromDefaults();

  // Similar to above, but causes sync to retry very quickly (see
  // polling_constants.h) when it encounters an error before exponential
  // backoff.
  //
  // *** NOTE *** This should only be used if kSyncShortInitialRetryOverride
  // was passed to command line.
  static BackoffDelayProvider* WithShortInitialRetryOverride();

  virtual ~BackoffDelayProvider();

  // DDOS avoidance function.  Calculates how long we should wait before trying
  // again after a failed sync attempt, where the last delay was |base_delay|.
  // TODO(tim): Look at URLRequestThrottlerEntryInterface.
  virtual base::TimeDelta GetDelay(const base::TimeDelta& last_delay);

  // Helper to calculate the initial value for exponential backoff.
  // See possible values and comments in polling_constants.h.
  virtual base::TimeDelta GetInitialDelay(const ModelNeutralState& state) const;

 protected:
  BackoffDelayProvider(const base::TimeDelta& default_initial_backoff,
                       const base::TimeDelta& short_initial_backoff);

 private:
  const base::TimeDelta default_initial_backoff_;
  const base::TimeDelta short_initial_backoff_;

  DISALLOW_COPY_AND_ASSIGN(BackoffDelayProvider);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_BACKOFF_DELAY_PROVIDER_H_
