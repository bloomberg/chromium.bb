// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/sync_notifier_factory.h"

#include <string>

#include "base/logging.h"
#include "sync/notifier/non_blocking_invalidation_notifier.h"
#include "sync/notifier/p2p_notifier.h"
#include "sync/notifier/sync_notifier.h"

namespace sync_notifier {
namespace {

SyncNotifier* CreateDefaultSyncNotifier(
    const notifier::NotifierOptions& notifier_options,
    const InvalidationVersionMap& initial_max_invalidation_versions,
    const browser_sync::WeakHandle<InvalidationVersionTracker>&
        invalidation_version_tracker,
    const std::string& client_info) {
  if (notifier_options.notification_method == notifier::NOTIFICATION_P2P) {
    // TODO(rlarocque): Ideally, the notification target would be
    // NOTIFY_OTHERS.  There's no good reason to notify ourselves of our own
    // commits.  We self-notify for now only because the integration tests rely
    // on this behaviour.  See crbug.com/97780.
    return new P2PNotifier(notifier_options, NOTIFY_ALL);
  }

  return new NonBlockingInvalidationNotifier(
      notifier_options, initial_max_invalidation_versions,
      invalidation_version_tracker, client_info);
}

}  // namespace

// TODO(akalin): Remove the dependency on jingle if OS_ANDROID is defined.
SyncNotifierFactory::SyncNotifierFactory(
    const notifier::NotifierOptions& notifier_options,
    const std::string& client_info,
    const base::WeakPtr<InvalidationVersionTracker>&
        invalidation_version_tracker)
    : notifier_options_(notifier_options),
      client_info_(client_info),
      initial_max_invalidation_versions_(
          invalidation_version_tracker.get() ?
          invalidation_version_tracker->GetAllMaxVersions() :
          InvalidationVersionMap()),
      invalidation_version_tracker_(invalidation_version_tracker) {
}

SyncNotifierFactory::~SyncNotifierFactory() {
}

SyncNotifier* SyncNotifierFactory::CreateSyncNotifier() {
#if defined(OS_ANDROID)
  // Android uses ChromeSyncNotificationBridge exclusively.
  return NULL;
#else
  return CreateDefaultSyncNotifier(notifier_options_,
                                   initial_max_invalidation_versions_,
                                   invalidation_version_tracker_,
                                   client_info_);
#endif
}
}  // namespace sync_notifier
