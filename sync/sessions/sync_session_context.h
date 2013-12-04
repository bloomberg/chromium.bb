// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SyncSessionContext encapsulates the contextual information and engine
// components specific to a SyncSession.  Unlike the SyncSession, the context
// can be reused across several sync cycles.
//
// The context does not take ownership of its pointer members.  It's up to
// the surrounding classes to ensure those members remain valid while the
// context is in use.
//
// It can only be used from the SyncerThread.

#ifndef SYNC_SESSIONS_SYNC_SESSION_CONTEXT_H_
#define SYNC_SESSIONS_SYNC_SESSION_CONTEXT_H_

#include <map>
#include <string>
#include <vector>

#include "base/stl_util.h"
#include "sync/base/sync_export.h"
#include "sync/engine/sync_directory_commit_contributor.h"
#include "sync/engine/sync_directory_update_handler.h"
#include "sync/engine/sync_engine_event.h"
#include "sync/engine/syncer_types.h"
#include "sync/engine/traffic_recorder.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/debug_info_getter.h"

namespace syncer {

class ExtensionsActivity;
class ServerConnectionManager;

namespace syncable {
class Directory;
}

// Default number of items a client can commit in a single message.
static const int kDefaultMaxCommitBatchSize = 25;

namespace sessions {
class TestScopedSessionEventListener;

class SYNC_EXPORT_PRIVATE SyncSessionContext {
 public:
  SyncSessionContext(ServerConnectionManager* connection_manager,
                     syncable::Directory* directory,
                     const std::vector<ModelSafeWorker*>& workers,
                     ExtensionsActivity* extensions_activity,
                     const std::vector<SyncEngineEventListener*>& listeners,
                     DebugInfoGetter* debug_info_getter,
                     TrafficRecorder* traffic_recorder,
                     bool keystore_encryption_enabled,
                     bool client_enabled_pre_commit_update_avoidance,
                     const std::string& invalidator_client_id);

  ~SyncSessionContext();

  ServerConnectionManager* connection_manager() {
    return connection_manager_;
  }
  syncable::Directory* directory() {
    return directory_;
  }

  ModelTypeSet enabled_types() const {
    return enabled_types_;
  }

  void set_routing_info(const ModelSafeRoutingInfo& routing_info);

  UpdateHandlerMap* update_handler_map() {
    return &update_handler_map_;
  }

  CommitContributorMap* commit_contributor_map() {
    return &commit_contributor_map_;
  }

  ExtensionsActivity* extensions_activity() {
    return extensions_activity_.get();
  }

  DebugInfoGetter* debug_info_getter() {
    return debug_info_getter_;
  }

  // Talk notification status.
  void set_notifications_enabled(bool enabled) {
    notifications_enabled_ = enabled;
  }
  bool notifications_enabled() { return notifications_enabled_; }

  // Account name, set once a directory has been opened.
  void set_account_name(const std::string& name) {
    DCHECK(account_name_.empty());
    account_name_ = name;
  }
  const std::string& account_name() const { return account_name_; }

  void set_max_commit_batch_size(int batch_size) {
    max_commit_batch_size_ = batch_size;
  }
  int32 max_commit_batch_size() const { return max_commit_batch_size_; }

  void NotifyListeners(const SyncEngineEvent& event) {
    FOR_EACH_OBSERVER(SyncEngineEventListener, listeners_,
                      OnSyncEngineEvent(event));
  }

  TrafficRecorder* traffic_recorder() {
    return traffic_recorder_;
  }

  bool keystore_encryption_enabled() const {
    return keystore_encryption_enabled_;
  }

  void set_hierarchy_conflict_detected(bool value) {
    client_status_.set_hierarchy_conflict_detected(value);
  }

  const sync_pb::ClientStatus& client_status() const {
    return client_status_;
  }

  const std::string& invalidator_client_id() const {
    return invalidator_client_id_;
  }

  bool ShouldFetchUpdatesBeforeCommit() const {
    return !(server_enabled_pre_commit_update_avoidance_ ||
             client_enabled_pre_commit_update_avoidance_);
  }

  void set_server_enabled_pre_commit_update_avoidance(bool value) {
    server_enabled_pre_commit_update_avoidance_ = value;
  }

 private:
  // Rather than force clients to set and null-out various context members, we
  // extend our encapsulation boundary to scoped helpers that take care of this
  // once they are allocated. See definitions of these below.
  friend class TestScopedSessionEventListener;

  ObserverList<SyncEngineEventListener> listeners_;

  ServerConnectionManager* const connection_manager_;
  syncable::Directory* const directory_;

  // The set of enabled types.  Derrived from the routing info set with
  // set_routing_info().
  ModelTypeSet enabled_types_;

  // A map of 'update handlers', one for each enabled type.
  // This must be kept in sync with the routing info.  Our temporary solution to
  // that problem is to initialize this map in set_routing_info().
  UpdateHandlerMap update_handler_map_;

  // Deleter for the |update_handler_map_|.
  STLValueDeleter<UpdateHandlerMap> update_handler_deleter_;

  // A map of 'commit contributors', one for each enabled type.
  // This must be kept in sync with the routing info.  Our temporary solution to
  // that problem is to initialize this map in set_routing_info().
  CommitContributorMap commit_contributor_map_;

  // Deleter for the |commit_contributor_map_|.
  STLValueDeleter<CommitContributorMap> commit_contributor_deleter_;

  // The set of ModelSafeWorkers.  Used to execute tasks of various threads.
  std::map<ModelSafeGroup, scoped_refptr<ModelSafeWorker> > workers_;

  // We use this to stuff extensions activity into CommitMessages so the server
  // can correlate commit traffic with extension-related bookmark mutations.
  scoped_refptr<ExtensionsActivity> extensions_activity_;

  // Kept up to date with talk events to determine whether notifications are
  // enabled. True only if the notification channel is authorized and open.
  bool notifications_enabled_;

  // The name of the account being synced.
  std::string account_name_;

  // The server limits the number of items a client can commit in one batch.
  int max_commit_batch_size_;

  // We use this to get debug info to send to the server for debugging
  // client behavior on server side.
  DebugInfoGetter* const debug_info_getter_;

  TrafficRecorder* traffic_recorder_;

  // Satus information to be sent up to the server.
  sync_pb::ClientStatus client_status_;

  // Temporary variable while keystore encryption is behind a flag. True if
  // we should attempt performing keystore encryption related work, false if
  // the experiment is not enabled.
  bool keystore_encryption_enabled_;

  // This is a copy of the identifier the that the invalidations client used to
  // register itself with the invalidations server during startup.  We need to
  // provide this to the sync server when we make changes to enable it to
  // prevent us from receiving notifications of changes we make ourselves.
  const std::string invalidator_client_id_;

  // Flag to enable or disable the no pre-commit GetUpdates experiment.  When
  // this flag is set to false, the syncer has the option of not performing at
  // GetUpdates request when there is nothing to fetch.
  bool server_enabled_pre_commit_update_avoidance_;

  // If true, indicates that we've been passed a command-line flag to force
  // enable the pre-commit update avoidance experiment described above.
  const bool client_enabled_pre_commit_update_avoidance_;

  DISALLOW_COPY_AND_ASSIGN(SyncSessionContext);
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_SYNC_SESSION_CONTEXT_H_
