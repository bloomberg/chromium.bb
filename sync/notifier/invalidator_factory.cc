// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/invalidator_factory.h"

#include <string>

#include "base/logging.h"
#include "jingle/notifier/listener/push_client.h"
#include "sync/notifier/invalidator.h"
#include "sync/notifier/non_blocking_invalidator.h"
#include "sync/notifier/p2p_invalidator.h"

namespace syncer {
namespace {

Invalidator* CreateDefaultInvalidator(
    const notifier::NotifierOptions& notifier_options,
    const InvalidationStateMap& initial_invalidation_state_map,
    const std::string& invalidation_bootstrap_data,
    const WeakHandle<InvalidationStateTracker>& invalidation_state_tracker,
    const std::string& client_info) {
  if (notifier_options.notification_method == notifier::NOTIFICATION_P2P) {
    // TODO(rlarocque): Ideally, the notification target would be
    // NOTIFY_OTHERS.  There's no good reason to notify ourselves of our own
    // commits.  We self-notify for now only because the integration tests rely
    // on this behaviour.  See crbug.com/97780.
    return new P2PInvalidator(
        notifier::PushClient::CreateDefault(notifier_options),
        NOTIFY_ALL);
  }

  return new NonBlockingInvalidator(
      notifier_options, initial_invalidation_state_map,
      invalidation_bootstrap_data, invalidation_state_tracker, client_info);
}

}  // namespace

// TODO(akalin): Remove the dependency on jingle if OS_ANDROID is defined.
InvalidatorFactory::InvalidatorFactory(
    const notifier::NotifierOptions& notifier_options,
    const std::string& client_info,
    const base::WeakPtr<InvalidationStateTracker>&
        invalidation_state_tracker)
    : notifier_options_(notifier_options),
      client_info_(client_info),
      initial_invalidation_state_map_(
          invalidation_state_tracker.get() ?
          invalidation_state_tracker->GetAllInvalidationStates() :
          InvalidationStateMap()),
      invalidation_bootstrap_data_(
          invalidation_state_tracker.get() ?
          invalidation_state_tracker->GetBootstrapData() :
          std::string()),
      invalidation_state_tracker_(invalidation_state_tracker) {
}

InvalidatorFactory::~InvalidatorFactory() {
}

Invalidator* InvalidatorFactory::CreateInvalidator() {
#if defined(OS_ANDROID)
  // Android uses AndroidInvalidatorBridge instead.  See SyncManager
  // initialization code in SyncBackendHost for more information.
  return NULL;
#else
  return CreateDefaultInvalidator(notifier_options_,
                                  initial_invalidation_state_map_,
                                  invalidation_bootstrap_data_,
                                  invalidation_state_tracker_,
                                  client_info_);
#endif
}
}  // namespace syncer
