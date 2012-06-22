// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ENGINE_STATUS_SUMMARY_H_
#define SYNC_INTERNAL_API_PUBLIC_ENGINE_STATUS_SUMMARY_H_

#include <string>

#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/protocol/sync_protocol_error.h"

namespace csync {

// Status encapsulates detailed state about the internals of the SyncManager.
//
// This struct is closely tied to the AllStatus object which uses instances of
// it to track and report on the sync engine's internal state, and the functions
// in sync_ui_util.cc which convert the contents of this struct into a
// DictionaryValue used to populate the about:sync summary tab.
struct SyncStatus {
  SyncStatus();
  ~SyncStatus();

  // TODO(akalin): Replace this with a NotificationsDisabledReason
  // variable.
  bool notifications_enabled;  // True only if subscribed for notifications.

  // Notifications counters updated by the actions in synapi.
  int notifications_received;

  csync::SyncProtocolError sync_protocol_error;

  // Number of encryption conflicts counted during most recent sync cycle.
  int encryption_conflicts;

  // Number of hierarchy conflicts counted during most recent sync cycle.
  int hierarchy_conflicts;

  // Number of simple conflicts counted during most recent sync cycle.
  int simple_conflicts;

  // Number of items the server refused to commit due to conflict during most
  // recent sync cycle.
  int server_conflicts;

  // Number of items successfully committed during most recent sync cycle.
  int committed_count;

  bool syncing;
  // True after a client has done a first sync.
  bool initial_sync_ended;

  // Total updates available.  If zero, nothing left to download.
  int64 updates_available;
  // Total updates received by the syncer since browser start.
  int updates_received;
  // Total updates received that are echoes of our own changes.
  int reflected_updates_received;
  // Of updates_received, how many were tombstones.
  int tombstone_updates_received;

  // Total successful commits.
  int num_commits_total;

  // Total number of overwrites due to conflict resolver since browser start.
  int num_local_overwrites_total;
  int num_server_overwrites_total;

  // Count of empty and non empty getupdates;
  int nonempty_get_updates;
  int empty_get_updates;

  // Count of sync cycles that successfully committed items;
  int sync_cycles_with_commits;
  int sync_cycles_without_commits;

  // Count of useless and useful syncs we perform.
  int useless_sync_cycles;
  int useful_sync_cycles;

  // Encryption related.
  syncable::ModelTypeSet encrypted_types;
  bool cryptographer_ready;
  bool crypto_has_pending_keys;

  // Per-datatype throttled status.
  syncable::ModelTypeSet throttled_types;

  // The unique identifer for this client.
  std::string unique_id;
};

}  // namespace csync

#endif  // SYNC_INTERNAL_API_PUBLIC_ENGINE_STATUS_SUMMARY_H_
