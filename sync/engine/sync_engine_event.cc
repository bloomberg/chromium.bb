// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_engine_event.h"

namespace browser_sync {

SyncEngineEvent::SyncEngineEvent(EventCause cause) : what_happened(cause) {
}

SyncEngineEvent::~SyncEngineEvent() {}

}  // namespace browser_sync
