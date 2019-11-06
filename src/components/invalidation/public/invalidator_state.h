// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATOR_STATE_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATOR_STATE_H_

#include "components/invalidation/public/invalidation_export.h"

namespace syncer {

// Used by UMA histogram, so entries shouldn't be reordered or removed.
enum InvalidatorState {
  // Invalidations are fully working.
  INVALIDATIONS_ENABLED = 0,

  // Failure states
  // --------------
  // There is an underlying transient problem (e.g., network- or
  // XMPP-related).
  TRANSIENT_INVALIDATION_ERROR = 1,
  DEFAULT_INVALIDATION_ERROR = TRANSIENT_INVALIDATION_ERROR,
  // Our credentials have been rejected.
  INVALIDATION_CREDENTIALS_REJECTED = 2,

  // Called just before shutdown so handlers can unregister themselves.
  INVALIDATOR_SHUTTING_DOWN = 3,

  // The subscription to at least one topic has failed.
  SUBSCRIPTION_FAILURE = 4,

  // Invalidator was stopped.
  STOPPED = 5,

  // Starting was attempted, but failed due to absence of active account.
  NOT_STARTED_NO_ACTIVE_ACCOUNT = 6,

  // Starting was attempted, but failed due to absence of active account.
  NOT_STARTED_NO_REFRESH_TOKEN = 7,

  kMaxValue = NOT_STARTED_NO_REFRESH_TOKEN,
};

INVALIDATION_EXPORT const char* InvalidatorStateToString(
    InvalidatorState state);

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATOR_STATE_H_
