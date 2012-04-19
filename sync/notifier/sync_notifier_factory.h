// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_SYNC_NOTIFIER_FACTORY_H_
#define SYNC_NOTIFIER_SYNC_NOTIFIER_FACTORY_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "jingle/notifier/base/notifier_options.h"
#include "sync/notifier/invalidation_version_tracker.h"
#include "sync/util/weak_handle.h"

namespace sync_notifier {

class SyncNotifier;

// Class to instantiate various implementations of the SyncNotifier
// interface.
class SyncNotifierFactory {
 public:
  // |client_info| is a string identifying the client, e.g. a user
  // agent string.  |invalidation_version_tracker| may be NULL (for
  // tests).
  SyncNotifierFactory(
      const notifier::NotifierOptions& notifier_options,
      const std::string& client_info,
      const base::WeakPtr<InvalidationVersionTracker>&
          invalidation_version_tracker);
  ~SyncNotifierFactory();

  // Creates a sync notifier. Caller takes ownership of the returned
  // object.  However, the returned object must not outlive the
  // factory from which it was created.  Can be called on any thread.
  SyncNotifier* CreateSyncNotifier();

 private:
  const notifier::NotifierOptions notifier_options_;
  const std::string client_info_;
  const InvalidationVersionMap initial_max_invalidation_versions_;
  const browser_sync::WeakHandle<InvalidationVersionTracker>
      invalidation_version_tracker_;
};

}  // namespace sync_notifier

#endif  // SYNC_NOTIFIER_SYNC_NOTIFIER_FACTORY_H_
