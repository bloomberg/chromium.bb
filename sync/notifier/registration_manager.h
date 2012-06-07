// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that manages the registration of types for server-issued
// notifications.

#ifndef SYNC_NOTIFIER_REGISTRATION_MANAGER_H_
#define SYNC_NOTIFIER_REGISTRATION_MANAGER_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "base/timer.h"
#include "sync/internal_api/public/syncable/model_type.h"
// For invalidation::InvalidationListener::RegistrationState.
#include "google/cacheinvalidation/include/invalidation-listener.h"

namespace sync_notifier {

using ::invalidation::InvalidationListener;

// Manages the details of registering types for invalidation.
// Implements exponential backoff for repeated registration attempts
// to the invalidation client.
//
// TODO(akalin): Consolidate exponential backoff code.  Other
// implementations include the syncer thread (both versions) and XMPP
// retries.  The most sophisticated one is URLRequestThrottler; making
// that generic should work for everyone.
class RegistrationManager {
 public:
  // Constants for exponential backoff (used by tests).
  static const int kInitialRegistrationDelaySeconds;
  static const int kRegistrationDelayExponent;
  static const double kRegistrationDelayMaxJitter;
  static const int kMinRegistrationDelaySeconds;
  static const int kMaxRegistrationDelaySeconds;

  // Types used by testing functions.
  struct PendingRegistrationInfo {
    PendingRegistrationInfo();

    // Last time a registration request was actually sent.
    base::Time last_registration_request;
    // Time the registration was attempted.
    base::Time registration_attempt;
    // The calculated delay of the pending registration (which may be
    // negative).
    base::TimeDelta delay;
    // The delay of the timer, which should be max(delay, 0).
    base::TimeDelta actual_delay;
  };
  // Map from types with pending registrations to info about the
  // pending registration.
  typedef std::map<syncable::ModelType, PendingRegistrationInfo>
      PendingRegistrationMap;

  // Does not take ownership of |invalidation_client_|.
  explicit RegistrationManager(
      invalidation::InvalidationClient* invalidation_client);

  virtual ~RegistrationManager();

  // Registers all types included in the given set (that are not
  // already disabled) and sets all other types to be unregistered.
  void SetRegisteredTypes(syncable::ModelTypeSet types);

  // Marks the registration for the |model_type| lost and re-registers
  // it (unless it's disabled).
  void MarkRegistrationLost(syncable::ModelType model_type);

  // Marks registrations lost for all enabled types and re-registers
  // them.
  void MarkAllRegistrationsLost();

  // Marks the registration for the |model_type| permanently lost and
  // blocks any future registration attempts.
  void DisableType(syncable::ModelType model_type);

  // The functions below should only be used in tests.

  // Gets all currently-registered types.
  syncable::ModelTypeSet GetRegisteredTypes() const;

  // Gets all pending registrations and their next min delays.
  PendingRegistrationMap GetPendingRegistrations() const;

  // Run pending registrations immediately.
  void FirePendingRegistrationsForTest();

  // Calculate exponential backoff.  |jitter| must be Uniform[-1.0,
  // 1.0].
  static double CalculateBackoff(double retry_interval,
                                 double initial_retry_interval,
                                 double min_retry_interval,
                                 double max_retry_interval,
                                 double backoff_exponent,
                                 double jitter,
                                 double max_jitter);

 protected:
  // Overrideable for testing purposes.
  virtual double GetJitter();

 private:
  struct RegistrationStatus {
    RegistrationStatus();
    ~RegistrationStatus();

    // Calls registration_manager->DoRegister(model_type). (needed by
    // |registration_timer|).  Should only be called if |enabled| is
    // true.
    void DoRegister();

    // Sets |enabled| to false and resets other variables.
    void Disable();

    // The model type for which this is the status.
    syncable::ModelType model_type;
    // The parent registration manager.
    RegistrationManager* registration_manager;

    // Whether this data type should be registered.  Set to false if
    // we get a non-transient registration failure.
    bool enabled;
    // The current registration state.
    InvalidationListener::RegistrationState state;
    // When we last sent a registration request.
    base::Time last_registration_request;
    // When we last tried to register.
    base::Time last_registration_attempt;
    // The calculated delay of any pending registration (which may be
    // negative).
    base::TimeDelta delay;
    // The minimum time to wait until any next registration attempt.
    // Increased after each consecutive failure.
    base::TimeDelta next_delay;
    // The actual timer for registration.
    base::OneShotTimer<RegistrationStatus> registration_timer;
  };

  // Does nothing if the given type is disabled.  Otherwise, if
  // |is_retry| is not set, registers the given type immediately and
  // resets all backoff parameters.  If |is_retry| is set, registers
  // the given type at some point in the future and increases the
  // delay until the next retry.
  void TryRegisterType(syncable::ModelType model_type,
                       bool is_retry);

  // Registers the given type, which must be valid, immediately.
  // Updates |last_registration| in the appropriate
  // RegistrationStatus.  Should only be called by
  // RegistrationStatus::DoRegister().
  void DoRegisterType(syncable::ModelType model_type);

  // Unregisters the given type, which must be valid.
  void UnregisterType(syncable::ModelType model_type);

  // Returns true iff the given type, which must be valid, is registered.
  bool IsTypeRegistered(syncable::ModelType model_type) const;

  base::NonThreadSafe non_thread_safe_;
  RegistrationStatus registration_statuses_[syncable::MODEL_TYPE_COUNT];
  // Weak pointer.
  invalidation::InvalidationClient* invalidation_client_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationManager);
};

}  // namespace sync_notifier

#endif  // SYNC_NOTIFIER_REGISTRATION_MANAGER_H_
