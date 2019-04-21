// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/sync_cycle_event.h"

namespace syncer {

SyncCycleEvent::SyncCycleEvent(EventCause cause) : what_happened(cause) {}

SyncCycleEvent::~SyncCycleEvent() {}

}  // namespace syncer
