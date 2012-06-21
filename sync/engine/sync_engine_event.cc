// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_engine_event.h"

namespace csync {

SyncEngineEvent::SyncEngineEvent(EventCause cause) : what_happened(cause) {
}

SyncEngineEvent::~SyncEngineEvent() {}

}  // namespace csync
