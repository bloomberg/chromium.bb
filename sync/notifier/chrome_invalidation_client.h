// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple wrapper around invalidation::InvalidationClient that
// handles all the startup/shutdown details and hookups.

#ifndef SYNC_NOTIFIER_CHROME_INVALIDATION_CLIENT_H_
#define SYNC_NOTIFIER_CHROME_INVALIDATION_CLIENT_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "google/cacheinvalidation/include/invalidation-listener.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/internal_api/public/syncable/model_type_payload_map.h"
#include "sync/notifier/chrome_system_resources.h"
#include "sync/notifier/invalidation_state_tracker.h"
#include "sync/notifier/state_writer.h"
#include "sync/util/weak_handle.h"

namespace buzz {
class XmppTaskParentInterface;
}  // namespace buzz

namespace notifier {
class PushClient;
}  // namespace notifier

namespace sync_notifier {

using invalidation::InvalidationListener;

class RegistrationManager;

// ChromeInvalidationClient is not thread-safe and lives on the sync
// thread.
class ChromeInvalidationClient
    : public InvalidationListener,
      public StateWriter,
      public base::NonThreadSafe {
 public:
  class Listener {
   public:
    virtual ~Listener();

    virtual void OnInvalidate(
        const syncable::ModelTypePayloadMap& type_payloads) = 0;

    virtual void OnSessionStatusChanged(bool has_session) = 0;
  };

  explicit ChromeInvalidationClient(
      scoped_ptr<notifier::PushClient> push_client);

  // Calls Stop().
  virtual ~ChromeInvalidationClient();

  // Does not take ownership of |listener| or |state_writer|.
  // |invalidation_state_tracker| must be initialized.
  void Start(
      const std::string& client_id, const std::string& client_info,
      const std::string& state,
      const InvalidationVersionMap& initial_max_invalidation_versions,
      const browser_sync::WeakHandle<InvalidationStateTracker>&
          invalidation_state_tracker,
      Listener* listener);

  void UpdateCredentials(const std::string& email, const std::string& token);

  // Register the sync types that we're interested in getting
  // notifications for.  May be called at any time.
  void RegisterTypes(syncable::ModelTypeSet types);

  virtual void WriteState(const std::string& state) OVERRIDE;

  // invalidation::InvalidationListener implementation.
  virtual void Ready(
      invalidation::InvalidationClient* client) OVERRIDE;
  virtual void Invalidate(
      invalidation::InvalidationClient* client,
      const invalidation::Invalidation& invalidation,
      const invalidation::AckHandle& ack_handle) OVERRIDE;
  virtual void InvalidateUnknownVersion(
      invalidation::InvalidationClient* client,
      const invalidation::ObjectId& object_id,
      const invalidation::AckHandle& ack_handle) OVERRIDE;
  virtual void InvalidateAll(
      invalidation::InvalidationClient* client,
      const invalidation::AckHandle& ack_handle) OVERRIDE;
  virtual void InformRegistrationStatus(
      invalidation::InvalidationClient* client,
      const invalidation::ObjectId& object_id,
      InvalidationListener::RegistrationState reg_state) OVERRIDE;
  virtual void InformRegistrationFailure(
      invalidation::InvalidationClient* client,
      const invalidation::ObjectId& object_id,
      bool is_transient,
      const std::string& error_message) OVERRIDE;
  virtual void ReissueRegistrations(
      invalidation::InvalidationClient* client,
      const std::string& prefix,
      int prefix_length) OVERRIDE;
  virtual void InformError(
      invalidation::InvalidationClient* client,
      const invalidation::ErrorInfo& error_info) OVERRIDE;

 private:
  friend class ChromeInvalidationClientTest;

  void Stop();

  void EmitInvalidation(
      syncable::ModelTypeSet types, const std::string& payload);

  ChromeSystemResources chrome_system_resources_;
  InvalidationVersionMap max_invalidation_versions_;
  browser_sync::WeakHandle<InvalidationStateTracker>
      invalidation_state_tracker_;
  Listener* listener_;
  scoped_ptr<invalidation::InvalidationClient> invalidation_client_;
  scoped_ptr<RegistrationManager> registration_manager_;
  // Stored to pass to |registration_manager_| on start.
  syncable::ModelTypeSet registered_types_;
  bool ticl_ready_;

  DISALLOW_COPY_AND_ASSIGN(ChromeInvalidationClient);
};

}  // namespace sync_notifier

#endif  // SYNC_NOTIFIER_CHROME_INVALIDATION_CLIENT_H_
