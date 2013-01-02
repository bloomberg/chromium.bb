// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_INVALIDATOR_STATE_H_
#define SYNC_NOTIFIER_INVALIDATOR_STATE_H_

#include "jingle/notifier/listener/push_client_observer.h"
#include "sync/base/sync_export.h"

namespace syncer {

enum InvalidatorState {
  // Failure states
  // --------------
  // There is an underlying transient problem (e.g., network- or
  // XMPP-related).
  TRANSIENT_INVALIDATION_ERROR,
  DEFAULT_INVALIDATION_ERROR = TRANSIENT_INVALIDATION_ERROR,
  // Our credentials have been rejected.
  INVALIDATION_CREDENTIALS_REJECTED,

  // Invalidations are fully working.
  INVALIDATIONS_ENABLED
};

SYNC_EXPORT const char* InvalidatorStateToString(InvalidatorState state);

InvalidatorState FromNotifierReason(
    notifier::NotificationsDisabledReason reason);

// Should not be called when |state| == INVALIDATIONS_ENABLED.
SYNC_EXPORT_PRIVATE notifier::NotificationsDisabledReason
    ToNotifierReasonForTest(InvalidatorState state);

}  // namespace syncer

#endif  // SYNC_NOTIFIER_INVALIDATOR_STATE_H_
