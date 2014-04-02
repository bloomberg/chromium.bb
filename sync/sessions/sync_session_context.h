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

#include <string>

#include "sync/base/sync_export.h"
#include "sync/engine/sync_engine_event_listener.h"
#include "sync/sessions/debug_info_getter.h"
#include "sync/sessions/model_type_registry.h"

namespace syncer {

class ExtensionsActivity;
class ModelTypeRegistry;
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
  SyncSessionContext(
      ServerConnectionManager* connection_manager,
      syncable::Directory* directory,
      ExtensionsActivity* extensions_activity,
      const std::vector<SyncEngineEventListener*>& listeners,
      DebugInfoGetter* debug_info_getter,
      ModelTypeRegistry* model_type_registry,
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

  ModelTypeSet GetEnabledTypes() const;

  void SetRoutingInfo(const ModelSafeRoutingInfo& routing_info);

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

  ObserverList<SyncEngineEventListener>* listeners() {
    return &listeners_;
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

  ModelTypeRegistry* model_type_registry() {
    return model_type_registry_;
  }

 private:
  // Rather than force clients to set and null-out various context members, we
  // extend our encapsulation boundary to scoped helpers that take care of this
  // once they are allocated. See definitions of these below.
  friend class TestScopedSessionEventListener;

  ObserverList<SyncEngineEventListener> listeners_;

  ServerConnectionManager* const connection_manager_;
  syncable::Directory* const directory_;

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

  ModelTypeRegistry* model_type_registry_;

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
