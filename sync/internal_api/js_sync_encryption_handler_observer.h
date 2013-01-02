// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_JS_SYNC_ENCRYPTION_HANDLER_OBSERVER_H_
#define SYNC_INTERNAL_API_JS_SYNC_ENCRYPTION_HANDLER_OBSERVER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/sync_encryption_handler.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/protocol/sync_protocol_error.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace syncer {

class JsEventDetails;
class JsEventHandler;

// Routes SyncEncryptionHandler events to a JsEventHandler.
class SYNC_EXPORT_PRIVATE JsSyncEncryptionHandlerObserver
    : public SyncEncryptionHandler::Observer {
 public:
  JsSyncEncryptionHandlerObserver();
  virtual ~JsSyncEncryptionHandlerObserver();

  void SetJsEventHandler(const WeakHandle<JsEventHandler>& event_handler);

  // SyncEncryptionHandlerObserver::Observer implementation.
  virtual void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) OVERRIDE;
  virtual void OnPassphraseAccepted() OVERRIDE;
  virtual void OnBootstrapTokenUpdated(
      const std::string& bootstrap_token,
      BootstrapTokenType type) OVERRIDE;
  virtual void OnEncryptedTypesChanged(
      ModelTypeSet encrypted_types,
      bool encrypt_everything) OVERRIDE;
  virtual void OnEncryptionComplete() OVERRIDE;
  virtual void OnCryptographerStateChanged(
      Cryptographer* cryptographer) OVERRIDE;
  virtual void OnPassphraseTypeChanged(
      PassphraseType type,
      base::Time explicit_passphrase_time) OVERRIDE;

 private:
  void HandleJsEvent(const tracked_objects::Location& from_here,
                    const std::string& name, const JsEventDetails& details);

  WeakHandle<JsEventHandler> event_handler_;

  DISALLOW_COPY_AND_ASSIGN(JsSyncEncryptionHandlerObserver);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_JS_SYNC_ENCRYPTION_HANDLER_OBSERVER_H_
