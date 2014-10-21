// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_JS_SYNC_MANAGER_OBSERVER_H_
#define SYNC_INTERNAL_API_JS_SYNC_MANAGER_OBSERVER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/protocol/sync_protocol_error.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace syncer {

class JsEventDetails;
class JsEventHandler;

// Routes SyncManager events to a JsEventHandler.
class SYNC_EXPORT_PRIVATE JsSyncManagerObserver : public SyncManager::Observer {
 public:
  JsSyncManagerObserver();
  ~JsSyncManagerObserver() override;

  void SetJsEventHandler(const WeakHandle<JsEventHandler>& event_handler);

  // SyncManager::Observer implementation.
  void OnSyncCycleCompleted(
      const sessions::SyncSessionSnapshot& snapshot) override;
  void OnConnectionStatusChange(ConnectionStatus status) override;
  void OnInitializationComplete(
      const WeakHandle<JsBackend>& js_backend,
      const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
      bool success,
      syncer::ModelTypeSet restored_types) override;
  void OnActionableError(const SyncProtocolError& sync_protocol_error) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;
  void OnMigrationRequested(syncer::ModelTypeSet types) override;

 private:
  void HandleJsEvent(const tracked_objects::Location& from_here,
                    const std::string& name, const JsEventDetails& details);

  WeakHandle<JsEventHandler> event_handler_;

  DISALLOW_COPY_AND_ASSIGN(JsSyncManagerObserver);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_JS_SYNC_MANAGER_OBSERVER_H_
