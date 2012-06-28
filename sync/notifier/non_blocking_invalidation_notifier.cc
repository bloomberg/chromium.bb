// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/non_blocking_invalidation_notifier.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "jingle/notifier/listener/push_client.h"
#include "sync/notifier/invalidation_notifier.h"

namespace syncer {

class NonBlockingInvalidationNotifier::Core
    : public base::RefCountedThreadSafe<NonBlockingInvalidationNotifier::Core>,
      // SyncNotifierObserver to observe the InvalidationNotifier we create.
      public SyncNotifierObserver {
 public:
  // Called on parent thread.  |delegate_observer| should be
  // initialized.
  explicit Core(
      const syncer::WeakHandle<SyncNotifierObserver>&
          delegate_observer);

  // Helpers called on I/O thread.
  void Initialize(
      const notifier::NotifierOptions& notifier_options,
      const InvalidationVersionMap& initial_max_invalidation_versions,
      const std::string& initial_invalidation_state,
      const syncer::WeakHandle<InvalidationStateTracker>&
          invalidation_state_tracker,
      const std::string& client_info);
  void Teardown();
  void SetUniqueId(const std::string& unique_id);
  void SetStateDeprecated(const std::string& state);
  void UpdateCredentials(const std::string& email, const std::string& token);
  void UpdateEnabledTypes(syncable::ModelTypeSet enabled_types);

  // SyncNotifierObserver implementation (all called on I/O thread by
  // InvalidationNotifier).
  virtual void OnNotificationsEnabled() OVERRIDE;
  virtual void OnNotificationsDisabled(
      NotificationsDisabledReason reason) OVERRIDE;
  virtual void OnIncomingNotification(
      const syncable::ModelTypePayloadMap& type_payloads,
      IncomingNotificationSource source) OVERRIDE;

 private:
  friend class
      base::RefCountedThreadSafe<NonBlockingInvalidationNotifier::Core>;
  // Called on parent or I/O thread.
  ~Core();

  // The variables below should be used only on the I/O thread.
  const syncer::WeakHandle<SyncNotifierObserver> delegate_observer_;
  scoped_ptr<InvalidationNotifier> invalidation_notifier_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

NonBlockingInvalidationNotifier::Core::Core(
    const syncer::WeakHandle<SyncNotifierObserver>&
        delegate_observer)
    : delegate_observer_(delegate_observer) {
  DCHECK(delegate_observer_.IsInitialized());
}

NonBlockingInvalidationNotifier::Core::~Core() {
}

void NonBlockingInvalidationNotifier::Core::Initialize(
    const notifier::NotifierOptions& notifier_options,
    const InvalidationVersionMap& initial_max_invalidation_versions,
    const std::string& initial_invalidation_state,
    const syncer::WeakHandle<InvalidationStateTracker>&
        invalidation_state_tracker,
    const std::string& client_info) {
  DCHECK(notifier_options.request_context_getter);
  DCHECK_EQ(notifier::NOTIFICATION_SERVER,
            notifier_options.notification_method);
  network_task_runner_ = notifier_options.request_context_getter->
      GetNetworkTaskRunner();
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_.reset(
      new InvalidationNotifier(
          notifier::PushClient::CreateDefaultOnIOThread(notifier_options),
          initial_max_invalidation_versions,
          initial_invalidation_state,
          invalidation_state_tracker,
          client_info));
  invalidation_notifier_->AddObserver(this);
}


void NonBlockingInvalidationNotifier::Core::Teardown() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->RemoveObserver(this);
  invalidation_notifier_.reset();
  network_task_runner_ = NULL;
}

void NonBlockingInvalidationNotifier::Core::SetUniqueId(
    const std::string& unique_id) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->SetUniqueId(unique_id);
}

void NonBlockingInvalidationNotifier::Core::SetStateDeprecated(
    const std::string& state) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->SetStateDeprecated(state);
}

void NonBlockingInvalidationNotifier::Core::UpdateCredentials(
    const std::string& email, const std::string& token) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->UpdateCredentials(email, token);
}

void NonBlockingInvalidationNotifier::Core::UpdateEnabledTypes(
    syncable::ModelTypeSet enabled_types) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->UpdateEnabledTypes(enabled_types);
}

void NonBlockingInvalidationNotifier::Core::OnNotificationsEnabled() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  delegate_observer_.Call(FROM_HERE,
                          &SyncNotifierObserver::OnNotificationsEnabled);
}

void NonBlockingInvalidationNotifier::Core::OnNotificationsDisabled(
    NotificationsDisabledReason reason) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  delegate_observer_.Call(
      FROM_HERE, &SyncNotifierObserver::OnNotificationsDisabled, reason);
}

void NonBlockingInvalidationNotifier::Core::OnIncomingNotification(
    const syncable::ModelTypePayloadMap& type_payloads,
    IncomingNotificationSource source) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  delegate_observer_.Call(FROM_HERE,
                          &SyncNotifierObserver::OnIncomingNotification,
                          type_payloads,
                          source);
}

NonBlockingInvalidationNotifier::NonBlockingInvalidationNotifier(
    const notifier::NotifierOptions& notifier_options,
    const InvalidationVersionMap& initial_max_invalidation_versions,
    const std::string& initial_invalidation_state,
    const syncer::WeakHandle<InvalidationStateTracker>&
        invalidation_state_tracker,
    const std::string& client_info)
        : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
          core_(
              new Core(syncer::MakeWeakHandle(
                  weak_ptr_factory_.GetWeakPtr()))),
          parent_task_runner_(
              base::ThreadTaskRunnerHandle::Get()),
          network_task_runner_(notifier_options.request_context_getter->
              GetNetworkTaskRunner()) {
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &NonBlockingInvalidationNotifier::Core::Initialize,
              core_.get(),
              notifier_options,
              initial_max_invalidation_versions,
              initial_invalidation_state,
              invalidation_state_tracker,
              client_info))) {
    NOTREACHED();
  }
}

NonBlockingInvalidationNotifier::~NonBlockingInvalidationNotifier() {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingInvalidationNotifier::Core::Teardown,
                     core_.get()))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidationNotifier::AddObserver(
    SyncNotifierObserver* observer) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  observers_.AddObserver(observer);
}

void NonBlockingInvalidationNotifier::RemoveObserver(
    SyncNotifierObserver* observer) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  observers_.RemoveObserver(observer);
}

void NonBlockingInvalidationNotifier::SetUniqueId(
    const std::string& unique_id) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingInvalidationNotifier::Core::SetUniqueId,
                     core_.get(), unique_id))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidationNotifier::SetStateDeprecated(
    const std::string& state) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &NonBlockingInvalidationNotifier::Core::SetStateDeprecated,
              core_.get(), state))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidationNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingInvalidationNotifier::Core::UpdateCredentials,
                     core_.get(), email, token))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidationNotifier::UpdateEnabledTypes(
    syncable::ModelTypeSet enabled_types) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingInvalidationNotifier::Core::UpdateEnabledTypes,
                     core_.get(), enabled_types))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidationNotifier::SendNotification(
    syncable::ModelTypeSet changed_types) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  // InvalidationClient doesn't implement SendNotification(), so no
  // need to forward on the call.
}

void NonBlockingInvalidationNotifier::OnNotificationsEnabled() {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_,
                    OnNotificationsEnabled());
}

void NonBlockingInvalidationNotifier::OnNotificationsDisabled(
    NotificationsDisabledReason reason) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_,
                    OnNotificationsDisabled(reason));
}

void NonBlockingInvalidationNotifier::OnIncomingNotification(
        const syncable::ModelTypePayloadMap& type_payloads,
        IncomingNotificationSource source) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_,
                    OnIncomingNotification(type_payloads, source));
}

}  // namespace syncer
