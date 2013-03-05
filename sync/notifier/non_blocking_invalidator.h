// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of SyncNotifier that wraps InvalidationNotifier
// on its own thread.

#ifndef SYNC_NOTIFIER_NON_BLOCKING_INVALIDATOR_H_
#define SYNC_NOTIFIER_NON_BLOCKING_INVALIDATOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "jingle/notifier/base/notifier_options.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/notifier/invalidation_handler.h"
#include "sync/notifier/invalidation_state_tracker.h"
#include "sync/notifier/invalidator.h"
#include "sync/notifier/invalidator_registrar.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace syncer {

// TODO(akalin): Generalize the interface so it can use any Invalidator.
// (http://crbug.com/140409).
class SYNC_EXPORT_PRIVATE NonBlockingInvalidator
    : public Invalidator,
      // InvalidationHandler to "observe" our Core via WeakHandle.
      public InvalidationHandler {
 public:
  // |invalidation_state_tracker| must be initialized.
  NonBlockingInvalidator(
      const notifier::NotifierOptions& notifier_options,
      const InvalidationStateMap& initial_invalidation_state_map,
      const std::string& invalidation_bootstrap_data,
      const WeakHandle<InvalidationStateTracker>&
          invalidation_state_tracker,
      const std::string& client_info);

  virtual ~NonBlockingInvalidator();

  // Invalidator implementation.
  virtual void RegisterHandler(InvalidationHandler* handler) OVERRIDE;
  virtual void UpdateRegisteredIds(InvalidationHandler* handler,
                                   const ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterHandler(InvalidationHandler* handler) OVERRIDE;
  virtual void Acknowledge(const invalidation::ObjectId& id,
                           const AckHandle& ack_handle) OVERRIDE;
  virtual InvalidatorState GetInvalidatorState() const OVERRIDE;
  virtual void SetUniqueId(const std::string& unique_id) OVERRIDE;
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE;
  virtual void SendInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) OVERRIDE;

  // InvalidationHandler implementation.
  virtual void OnInvalidatorStateChange(InvalidatorState state) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) OVERRIDE;

 private:
  class Core;

  base::WeakPtrFactory<NonBlockingInvalidator> weak_ptr_factory_;

  InvalidatorRegistrar registrar_;

  // The real guts of NonBlockingInvalidator, which allows this class to live
  // completely on the parent thread.
  scoped_refptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> parent_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(NonBlockingInvalidator);
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_NON_BLOCKING_INVALIDATOR_H_
