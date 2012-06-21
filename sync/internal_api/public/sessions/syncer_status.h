// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SESSIONS_SYNCER_STATUS_H_
#define SYNC_INTERNAL_API_PUBLIC_SESSIONS_SYNCER_STATUS_H_
#pragma once

#include "base/basictypes.h"
#include "sync/internal_api/public/syncable/model_type.h"

namespace base {
class DictionaryValue;
}

namespace csync {
namespace sessions {

// Data pertaining to the status of an active Syncer object.
struct SyncerStatus {
  SyncerStatus();
  ~SyncerStatus();

  // Caller takes ownership of the returned dictionary.
  base::DictionaryValue* ToValue() const;

  // True when we get such an INVALID_STORE error from the server.
  bool invalid_store;
  int num_successful_commits;
  // This is needed for monitoring extensions activity.
  int num_successful_bookmark_commits;

  // Download event counters.
  int num_updates_downloaded_total;
  int num_tombstone_updates_downloaded_total;
  int num_reflected_updates_downloaded_total;

  // If the syncer encountered a MIGRATION_DONE code, these are the types that
  // the client must now "migrate", by purging and re-downloading all updates.
  syncable::ModelTypeSet types_needing_local_migration;

  // Overwrites due to conflict resolution counters.
  int num_local_overwrites;
  int num_server_overwrites;
};

}  // namespace sessions
}  // namespace csync

#endif  // SYNC_INTERNAL_API_PUBLIC_SESSIONS_SYNCER_STATUS_H_
