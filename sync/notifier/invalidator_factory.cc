// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/invalidator_factory.h"

#include <string>

#include "base/base64.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "jingle/notifier/listener/push_client.h"
#include "sync/notifier/invalidator.h"
#include "sync/notifier/non_blocking_invalidator.h"
#include "sync/notifier/p2p_invalidator.h"

namespace syncer {
namespace {

Invalidator* CreateDefaultInvalidator(
    const notifier::NotifierOptions& notifier_options,
    const std::string& invalidator_client_id,
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
        invalidator_client_id,
        NOTIFY_ALL);
  }

  return new NonBlockingInvalidator(
      notifier_options, invalidator_client_id, initial_invalidation_state_map,
      invalidation_bootstrap_data, invalidation_state_tracker, client_info);
}

std::string GenerateInvalidatorClientId() {
  // Generate a GUID with 128 bits worth of base64-encoded randomness.
  // This format is similar to that of sync's cache_guid.
  const int kGuidBytes = 128 / 8;
  std::string guid;
  base::Base64Encode(base::RandBytesAsString(kGuidBytes), &guid);
  return guid;
}

}  // namespace

// TODO(akalin): Remove the dependency on jingle if OS_ANDROID is defined.
InvalidatorFactory::InvalidatorFactory(
    const notifier::NotifierOptions& notifier_options,
    const std::string& client_info,
    const base::WeakPtr<InvalidationStateTracker>&
        invalidation_state_tracker)
    : notifier_options_(notifier_options),
      client_info_(client_info) {
  if (!invalidation_state_tracker.get()) {
    return;
  }

  // TODO(rlarocque): This is not the most obvious place for client ID
  // generation code.  We should try to find a better place for it when we
  // refactor the invalidator into its own service.
  if (invalidation_state_tracker->GetInvalidatorClientId().empty()) {
    // This also clears any existing state.  We can't reuse old invalidator
    // state with the new ID anyway.
    invalidation_state_tracker->SetInvalidatorClientId(
        GenerateInvalidatorClientId());
  }

  initial_invalidation_state_map_ =
      invalidation_state_tracker->GetAllInvalidationStates();
  invalidator_client_id_ =
      invalidation_state_tracker->GetInvalidatorClientId();
  invalidation_bootstrap_data_ = invalidation_state_tracker->GetBootstrapData();
  invalidation_state_tracker_ = WeakHandle<InvalidationStateTracker>(
      invalidation_state_tracker);
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
                                  invalidator_client_id_,
                                  initial_invalidation_state_map_,
                                  invalidation_bootstrap_data_,
                                  invalidation_state_tracker_,
                                  client_info_);
#endif
}

std::string InvalidatorFactory::GetInvalidatorClientId() const {
  return invalidator_client_id_;
}

}  // namespace syncer
