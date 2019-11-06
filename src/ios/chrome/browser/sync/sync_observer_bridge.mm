// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/sync_observer_bridge.h"

#include "base/logging.h"
#include "components/sync/driver/sync_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

SyncObserverBridge::SyncObserverBridge(id<SyncObserverModelBridge> delegate,
                                       syncer::SyncService* sync_service)
    : delegate_(delegate), scoped_observer_(this) {
  DCHECK(delegate);
  if (sync_service)
    scoped_observer_.Add(sync_service);
}

SyncObserverBridge::~SyncObserverBridge() {
}

void SyncObserverBridge::OnStateChanged(syncer::SyncService* sync) {
  [delegate_ onSyncStateChanged];
}

void SyncObserverBridge::OnSyncConfigurationCompleted(
    syncer::SyncService* sync) {
  if ([delegate_ respondsToSelector:@selector(onSyncConfigurationCompleted)])
    [delegate_ onSyncConfigurationCompleted];
}
