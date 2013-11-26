// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/non_blocking_invalidator.h"

#include <cstddef>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "jingle/notifier/listener/push_client.h"
#include "sync/notifier/invalidation_notifier.h"
#include "sync/notifier/object_id_invalidation_map.h"

namespace syncer {

class NonBlockingInvalidator::Core
    : public base::RefCountedThreadSafe<NonBlockingInvalidator::Core>,
      // InvalidationHandler to observe the InvalidationNotifier we create.
      public InvalidationHandler {
 public:
  // Called on parent thread.  |delegate_observer| should be
  // initialized.
  explicit Core(
      const WeakHandle<InvalidationHandler>& delegate_observer);

  // Helpers called on I/O thread.
  void Initialize(
      const notifier::NotifierOptions& notifier_options,
      const std::string& invalidator_client_id,
      const UnackedInvalidationsMap& saved_invalidations,
      const std::string& invalidation_bootstrap_data,
      const WeakHandle<InvalidationStateTracker>& invalidation_state_tracker,
      const std::string& client_info);
  void Teardown();
  void UpdateRegisteredIds(const ObjectIdSet& ids);
  void UpdateCredentials(const std::string& email, const std::string& token);

  // InvalidationHandler implementation (all called on I/O thread by
  // InvalidationNotifier).
  virtual void OnInvalidatorStateChange(InvalidatorState reason) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) OVERRIDE;

 private:
  friend class
      base::RefCountedThreadSafe<NonBlockingInvalidator::Core>;
  // Called on parent or I/O thread.
  virtual ~Core();

  // The variables below should be used only on the I/O thread.
  const WeakHandle<InvalidationHandler> delegate_observer_;
  scoped_ptr<InvalidationNotifier> invalidation_notifier_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

NonBlockingInvalidator::Core::Core(
    const WeakHandle<InvalidationHandler>& delegate_observer)
    : delegate_observer_(delegate_observer) {
  DCHECK(delegate_observer_.IsInitialized());
}

NonBlockingInvalidator::Core::~Core() {
}

void NonBlockingInvalidator::Core::Initialize(
    const notifier::NotifierOptions& notifier_options,
    const std::string& invalidator_client_id,
    const UnackedInvalidationsMap& saved_invalidations,
    const std::string& invalidation_bootstrap_data,
    const WeakHandle<InvalidationStateTracker>& invalidation_state_tracker,
    const std::string& client_info) {
  DCHECK(notifier_options.request_context_getter.get());
  DCHECK_EQ(notifier::NOTIFICATION_SERVER,
            notifier_options.notification_method);
  network_task_runner_ = notifier_options.request_context_getter->
      GetNetworkTaskRunner();
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_.reset(
      new InvalidationNotifier(
          notifier::PushClient::CreateDefaultOnIOThread(notifier_options),
          invalidator_client_id,
          saved_invalidations,
          invalidation_bootstrap_data,
          invalidation_state_tracker,
          client_info));
  invalidation_notifier_->RegisterHandler(this);
}

void NonBlockingInvalidator::Core::Teardown() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->UnregisterHandler(this);
  invalidation_notifier_.reset();
  network_task_runner_ = NULL;
}

void NonBlockingInvalidator::Core::UpdateRegisteredIds(const ObjectIdSet& ids) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->UpdateRegisteredIds(this, ids);
}

void NonBlockingInvalidator::Core::UpdateCredentials(const std::string& email,
                                                     const std::string& token) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->UpdateCredentials(email, token);
}

void NonBlockingInvalidator::Core::OnInvalidatorStateChange(
    InvalidatorState reason) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  delegate_observer_.Call(
      FROM_HERE, &InvalidationHandler::OnInvalidatorStateChange, reason);
}

void NonBlockingInvalidator::Core::OnIncomingInvalidation(
    const ObjectIdInvalidationMap& invalidation_map) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  delegate_observer_.Call(FROM_HERE,
                          &InvalidationHandler::OnIncomingInvalidation,
                          invalidation_map);
}

NonBlockingInvalidator::NonBlockingInvalidator(
    const notifier::NotifierOptions& notifier_options,
    const std::string& invalidator_client_id,
    const UnackedInvalidationsMap& saved_invalidations,
    const std::string& invalidation_bootstrap_data,
    const WeakHandle<InvalidationStateTracker>&
        invalidation_state_tracker,
    const std::string& client_info)
    : parent_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      network_task_runner_(
          notifier_options.request_context_getter->GetNetworkTaskRunner()),
      weak_ptr_factory_(this) {
  core_ = new Core(MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()));

  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &NonBlockingInvalidator::Core::Initialize,
              core_.get(),
              notifier_options,
              invalidator_client_id,
              saved_invalidations,
              invalidation_bootstrap_data,
              invalidation_state_tracker,
              client_info))) {
    NOTREACHED();
  }
}

NonBlockingInvalidator::~NonBlockingInvalidator() {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingInvalidator::Core::Teardown,
                     core_.get()))) {
    DVLOG(1) << "Network thread stopped before invalidator is destroyed.";
  }
}

void NonBlockingInvalidator::RegisterHandler(InvalidationHandler* handler) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  registrar_.RegisterHandler(handler);
}

void NonBlockingInvalidator::UpdateRegisteredIds(InvalidationHandler* handler,
                                                 const ObjectIdSet& ids) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  registrar_.UpdateRegisteredIds(handler, ids);
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &NonBlockingInvalidator::Core::UpdateRegisteredIds,
              core_.get(),
              registrar_.GetAllRegisteredIds()))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidator::UnregisterHandler(InvalidationHandler* handler) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  registrar_.UnregisterHandler(handler);
}

InvalidatorState NonBlockingInvalidator::GetInvalidatorState() const {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  return registrar_.GetInvalidatorState();
}

void NonBlockingInvalidator::UpdateCredentials(const std::string& email,
                                               const std::string& token) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingInvalidator::Core::UpdateCredentials,
                     core_.get(), email, token))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidator::OnInvalidatorStateChange(InvalidatorState state) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  registrar_.UpdateInvalidatorState(state);
}

void NonBlockingInvalidator::OnIncomingInvalidation(
        const ObjectIdInvalidationMap& invalidation_map) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

}  // namespace syncer
