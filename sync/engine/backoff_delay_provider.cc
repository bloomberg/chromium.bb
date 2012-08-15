// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/backoff_delay_provider.h"

#include "base/rand_util.h"
#include "sync/internal_api/public/engine/polling_constants.h"
#include "sync/internal_api/public/sessions/model_neutral_state.h"
#include "sync/internal_api/public/util/syncer_error.h"

using base::TimeDelta;

namespace syncer {

// static
BackoffDelayProvider* BackoffDelayProvider::FromDefaults() {
  return new BackoffDelayProvider(
      TimeDelta::FromSeconds(kInitialBackoffRetrySeconds),
      TimeDelta::FromSeconds(kInitialBackoffShortRetrySeconds));
}

// static
BackoffDelayProvider* BackoffDelayProvider::WithShortInitialRetryOverride() {
  return new BackoffDelayProvider(
      TimeDelta::FromSeconds(kInitialBackoffShortRetrySeconds),
      TimeDelta::FromSeconds(kInitialBackoffShortRetrySeconds));
}

BackoffDelayProvider::BackoffDelayProvider(
    const base::TimeDelta& default_initial_backoff,
    const base::TimeDelta& short_initial_backoff)
    : default_initial_backoff_(default_initial_backoff),
      short_initial_backoff_(short_initial_backoff) {
}

BackoffDelayProvider::~BackoffDelayProvider() {}

TimeDelta BackoffDelayProvider::GetDelay(const base::TimeDelta& last_delay) {
  if (last_delay.InSeconds() >= kMaxBackoffSeconds)
    return TimeDelta::FromSeconds(kMaxBackoffSeconds);

  // This calculates approx. base_delay_seconds * 2 +/- base_delay_seconds / 2
  int64 backoff_s =
      std::max(static_cast<int64>(1),
               last_delay.InSeconds() * kBackoffRandomizationFactor);

  // Flip a coin to randomize backoff interval by +/- 50%.
  int rand_sign = base::RandInt(0, 1) * 2 - 1;

  // Truncation is adequate for rounding here.
  backoff_s = backoff_s +
      (rand_sign * (last_delay.InSeconds() / kBackoffRandomizationFactor));

  // Cap the backoff interval.
  backoff_s = std::max(static_cast<int64>(1),
                       std::min(backoff_s, kMaxBackoffSeconds));

  return TimeDelta::FromSeconds(backoff_s);
}

TimeDelta BackoffDelayProvider::GetInitialDelay(
    const sessions::ModelNeutralState& state) const {
  if (SyncerErrorIsError(state.last_get_key_result))
    return default_initial_backoff_;
  // Note: If we received a MIGRATION_DONE on download updates, then commit
  // should not have taken place.  Moreover, if we receive a MIGRATION_DONE
  // on commit, it means that download updates succeeded.  Therefore, we only
  // need to check if either code is equal to SERVER_RETURN_MIGRATION_DONE,
  // and not if there were any more serious errors requiring the long retry.
  if (state.last_download_updates_result == SERVER_RETURN_MIGRATION_DONE ||
      state.commit_result == SERVER_RETURN_MIGRATION_DONE) {
    return short_initial_backoff_;
  }

  return default_initial_backoff_;
}

}  // namespace syncer
