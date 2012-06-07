// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_COMMIT_H_
#define SYNC_ENGINE_COMMIT_H_
#pragma once

#include "sync/internal_api/public/util/syncer_error.h"

namespace browser_sync {

namespace sessions {
class SyncSession;
}

class Syncer;

// This function will commit batches of unsynced items to the server until the
// number of unsynced and ready to commit items reaches zero or an error is
// encountered.  A request to exit early will be treated as an error and will
// abort any blocking operations.
//
// The Syncer parameter is provided only for access to its ExitRequested()
// method.  This is technically unnecessary since an early exit request should
// be detected as we attempt to contact the sync server.
//
// The SyncSession parameter contains pointers to various bits of state,
// including the syncable::Directory that contains all sync items and the
// ServerConnectionManager used to contact the server.
SyncerError BuildAndPostCommits(
    Syncer* syncer,
    sessions::SyncSession* session);

}  // namespace browser_sync

#endif  // SYNC_ENGINE_COMMIT_H_
