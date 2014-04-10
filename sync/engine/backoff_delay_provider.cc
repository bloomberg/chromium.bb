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
      TimeDelta::FromSeconds(kInitialBackoffImmediateRetrySeconds));
}

// static
BackoffDelayProvider* BackoffDelayProvider::WithShortInitialRetryOverride() {
  return new BackoffDelayProvider(
      TimeDelta::FromSeconds(kInitialBackoffShortRetrySeconds),
      TimeDelta::FromSeconds(kInitialBackoffImmediateRetrySeconds));
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
  // NETWORK_CONNECTION_UNAVAILABLE implies we did not even manage to hit the
  // wire; the failure occurred locally. Note that if commit_result is *not*
  // UNSET, this implies download_updates_result succeeded.  Also note that
  // last_get_key_result is coupled to last_download_updates_result in that
  // they are part of the same GetUpdates request, so we only check if
  // the download request is CONNECTION_UNAVAILABLE.
  //
  // TODO(tim): Should we treat NETWORK_IO_ERROR similarly? It's different
  // from CONNECTION_UNAVAILABLE in that a request may well have succeeded
  // in contacting the server (e.g we got a 200 back), but we failed
  // trying to parse the response (actual content length != HTTP response
  // header content length value).  For now since we're considering
  // merging this code to branches and I haven't audited all the
  // NETWORK_IO_ERROR cases carefully, I'm going to target the fix
  // very tightly (see bug chromium-os:35073). DIRECTORY_LOOKUP_FAILED is
  // another example of something that shouldn't backoff, though the
  // scheduler should probably be handling these cases differently. See
  // the TODO(rlarocque) in ScheduleNextSync.
  if (state.commit_result == NETWORK_CONNECTION_UNAVAILABLE ||
      state.last_download_updates_result == NETWORK_CONNECTION_UNAVAILABLE) {
    return short_initial_backoff_;
  }

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

  // If a datatype decides the GetUpdates must be retried (e.g. because the
  // context has been updated since the request), use the short delay.
  if (state.last_download_updates_result == DATATYPE_TRIGGERED_RETRY)
    return short_initial_backoff_;

  // When the server tells us we have a conflict, then we should download the
  // latest updates so we can see the conflict ourselves, resolve it locally,
  // then try again to commit.  Running another sync cycle will do all these
  // things.  There's no need to back off, we can do this immediately.
  //
  // TODO(sync): We shouldn't need to handle this in BackoffDelayProvider.
  // There should be a way to deal with protocol errors before we get to this
  // point.
  if (state.commit_result == SERVER_RETURN_CONFLICT)
    return short_initial_backoff_;

  return default_initial_backoff_;
}

}  // namespace syncer
