// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SyncSessionContext encapsulates the contextual information and engine
// components specific to a SyncSession. A context is accessible via
// a SyncSession so that session SyncerCommands and parts of the engine have
// a convenient way to access other parts. In this way it can be thought of as
// the surrounding environment for the SyncSession. The components of this
// environment are either valid or not valid for the entire context lifetime,
// or they are valid for explicitly scoped periods of time by using Scoped
// installation utilities found below. This means that the context assumes no
// ownership whatsoever of any object that was not created by the context
// itself.
//
// It can only be used from the SyncerThread.

#ifndef SYNC_SESSIONS_SYNC_SESSION_CONTEXT_H_
#define SYNC_SESSIONS_SYNC_SESSION_CONTEXT_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/time.h"
#include "sync/engine/sync_engine_event.h"
#include "sync/engine/syncer_types.h"
#include "sync/engine/traffic_recorder.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/sessions/debug_info_getter.h"

namespace syncable {
class Directory;
}

namespace csync {

class ConflictResolver;
class ExtensionsActivityMonitor;
class ServerConnectionManager;
class ThrottledDataTypeTracker;

// Default number of items a client can commit in a single message.
static const int kDefaultMaxCommitBatchSize = 25;

namespace sessions {
class ScopedSessionContextConflictResolver;
class TestScopedSessionEventListener;

class SyncSessionContext {
 public:
  SyncSessionContext(ServerConnectionManager* connection_manager,
                     syncable::Directory* directory,
                     const ModelSafeRoutingInfo& model_safe_routing_info,
                     const std::vector<ModelSafeWorker*>& workers,
                     ExtensionsActivityMonitor* extensions_activity_monitor,
                     ThrottledDataTypeTracker* throttled_data_type_tracker,
                     const std::vector<SyncEngineEventListener*>& listeners,
                     DebugInfoGetter* debug_info_getter,
                     csync::TrafficRecorder* traffic_recorder);
  ~SyncSessionContext();

  ConflictResolver* resolver() { return resolver_; }
  ServerConnectionManager* connection_manager() {
    return connection_manager_;
  }
  syncable::Directory* directory() {
    return directory_;
  }

  const ModelSafeRoutingInfo& routing_info() const {
    return routing_info_;
  }

  void set_routing_info(const ModelSafeRoutingInfo& routing_info) {
    routing_info_ = routing_info;
  }

  const std::vector<ModelSafeWorker*> workers() const {
    return workers_;
  }

  ExtensionsActivityMonitor* extensions_monitor() {
    return extensions_activity_monitor_;
  }

  ThrottledDataTypeTracker* throttled_data_type_tracker() {
    return throttled_data_type_tracker_;
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

  const ModelSafeRoutingInfo& previous_session_routing_info() const {
    return previous_session_routing_info_;
  }

  void set_previous_session_routing_info(const ModelSafeRoutingInfo& info) {
    previous_session_routing_info_ = info;
  }

  void NotifyListeners(const SyncEngineEvent& event) {
    FOR_EACH_OBSERVER(SyncEngineEventListener, listeners_,
                      OnSyncEngineEvent(event));
  }

  csync::TrafficRecorder* traffic_recorder() {
    return traffic_recorder_;
  }

 private:
  // Rather than force clients to set and null-out various context members, we
  // extend our encapsulation boundary to scoped helpers that take care of this
  // once they are allocated. See definitions of these below.
  friend class ScopedSessionContextConflictResolver;
  friend class TestScopedSessionEventListener;

  // This is installed by Syncer objects when needed and may be NULL.
  ConflictResolver* resolver_;

  ObserverList<SyncEngineEventListener> listeners_;

  ServerConnectionManager* const connection_manager_;
  syncable::Directory* const directory_;

  // A cached copy of SyncBackendRegistrar's routing info.
  // Must be updated manually when SBR's state is modified.
  ModelSafeRoutingInfo routing_info_;

  // The set of ModelSafeWorkers.  Used to execute tasks of various threads.
  const std::vector<ModelSafeWorker*> workers_;

  // We use this to stuff extensions activity into CommitMessages so the server
  // can correlate commit traffic with extension-related bookmark mutations.
  ExtensionsActivityMonitor* extensions_activity_monitor_;

  // Kept up to date with talk events to determine whether notifications are
  // enabled. True only if the notification channel is authorized and open.
  bool notifications_enabled_;

  // The name of the account being synced.
  std::string account_name_;

  // The server limits the number of items a client can commit in one batch.
  int max_commit_batch_size_;

  // Some routing info history to help us clean up types that get disabled
  // by the user.
  ModelSafeRoutingInfo previous_session_routing_info_;

  ThrottledDataTypeTracker* throttled_data_type_tracker_;

  // We use this to get debug info to send to the server for debugging
  // client behavior on server side.
  DebugInfoGetter* const debug_info_getter_;

  csync::TrafficRecorder* traffic_recorder_;

  DISALLOW_COPY_AND_ASSIGN(SyncSessionContext);
};

// Installs a ConflictResolver to a given session context for the lifetime of
// the ScopedSessionContextConflictResolver.  There should never be more than
// one ConflictResolver in the system, so it is an error to use this if the
// context already has a resolver.
class ScopedSessionContextConflictResolver {
 public:
  // Note: |context| and |resolver| should outlive |this|.
  ScopedSessionContextConflictResolver(SyncSessionContext* context,
                                       ConflictResolver* resolver)
      : context_(context), resolver_(resolver) {
    DCHECK(NULL == context->resolver_);
    context->resolver_ = resolver;
  }
  ~ScopedSessionContextConflictResolver() {
    context_->resolver_ = NULL;
  }

 private:
  SyncSessionContext* context_;
  ConflictResolver* resolver_;
  DISALLOW_COPY_AND_ASSIGN(ScopedSessionContextConflictResolver);
};

}  // namespace sessions
}  // namespace csync

#endif  // SYNC_SESSIONS_SYNC_SESSION_CONTEXT_H_
