// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_JS_SYNC_MANAGER_OBSERVER_H_
#define SYNC_INTERNAL_API_JS_SYNC_MANAGER_OBSERVER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/protocol/sync_protocol_error.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace csync {

class JsEventDetails;
class JsEventHandler;

// Routes SyncManager events to a JsEventHandler.
class JsSyncManagerObserver : public csync::SyncManager::Observer {
 public:
  JsSyncManagerObserver();
  virtual ~JsSyncManagerObserver();

  void SetJsEventHandler(const WeakHandle<JsEventHandler>& event_handler);

  // csync::SyncManager::Observer implementation.
  virtual void OnSyncCycleCompleted(
      const sessions::SyncSessionSnapshot& snapshot) OVERRIDE;
  virtual void OnConnectionStatusChange(
      csync::ConnectionStatus status) OVERRIDE;
  virtual void OnUpdatedToken(const std::string& token) OVERRIDE;
  virtual void OnPassphraseRequired(
      csync::PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) OVERRIDE;
  virtual void OnPassphraseAccepted() OVERRIDE;
  virtual void OnBootstrapTokenUpdated(
      const std::string& bootstrap_token) OVERRIDE;
  virtual void OnEncryptedTypesChanged(
      syncable::ModelTypeSet encrypted_types,
      bool encrypt_everything) OVERRIDE;
  virtual void OnEncryptionComplete() OVERRIDE;
  virtual void OnInitializationComplete(
      const WeakHandle<JsBackend>& js_backend, bool success) OVERRIDE;
  virtual void OnStopSyncingPermanently() OVERRIDE;
  virtual void OnActionableError(
      const csync::SyncProtocolError& sync_protocol_error) OVERRIDE;

 private:
  void HandleJsEvent(const tracked_objects::Location& from_here,
                    const std::string& name, const JsEventDetails& details);

  WeakHandle<JsEventHandler> event_handler_;

  DISALLOW_COPY_AND_ASSIGN(JsSyncManagerObserver);
};

}  // namespace csync

#endif  // SYNC_INTERNAL_API_JS_SYNC_MANAGER_OBSERVER_H_
