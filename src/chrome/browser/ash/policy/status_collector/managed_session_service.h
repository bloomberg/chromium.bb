// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_MANAGED_SESSION_SERVICE_H_
#define CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_MANAGED_SESSION_SERVICE_H_

#include "ash/components/login/auth/auth_status_consumer.h"
#include "ash/components/login/session/session_termination_manager.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager_base.h"

namespace policy {

class ManagedSessionService : public session_manager::SessionManagerObserver,
                              public ProfileObserver,
                              public chromeos::PowerManagerClient::Observer,
                              public ash::AuthStatusConsumer,
                              public ash::UserAuthenticatorObserver,
                              public ash::SessionTerminationManager::Observer,
                              public user_manager::UserManager::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Occurs when a user's login attempt fails.
    virtual void OnLoginFailure(const ash::AuthFailure& error) {}

    // Occurs when a user has logged in.
    virtual void OnLogin(Profile* profile) {}

    // Occurs when a user has logged out.
    // TODO(b/194215634):: Check if this function can be replaced by
    // `OnSessionTerminationStarted`
    virtual void OnLogout(Profile* profile) {}

    // Occurs when the active user has locked the user session.
    virtual void OnLocked() {}

    // Occurs when the active user has unlocked the user session.
    virtual void OnUnlocked() {}

    // Occurs when the device recovers from a suspend state, where
    // |suspend_time| is the time when the suspend state
    // first occurred. Short duration suspends are not reported.
    virtual void OnResumeActive(base::Time suspend_time) {}

    // Occurs in the beginning of the session termination process.
    virtual void OnSessionTerminationStarted(const user_manager::User* user) {}

    // Occurs just before a user's account will be removed.
    virtual void OnUserToBeRemoved(const AccountId& account_id) {}

    // Occurs after a user's account is removed.
    virtual void OnUserRemoved(const AccountId& account_id,
                               user_manager::UserRemovalReason) {}
  };

  explicit ManagedSessionService(
      base::Clock* clock = base::DefaultClock::GetInstance());
  ManagedSessionService(const ManagedSessionService&) = delete;
  ManagedSessionService& operator=(const ManagedSessionService&) = delete;
  ~ManagedSessionService() override;

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

  // session_manager::SessionManagerObserver::Observer
  void OnSessionStateChanged() override;
  void OnUserProfileLoaded(const AccountId& account_id) override;

  // user_manager::Observer
  void OnUserToBeRemoved(const AccountId& account_id) override;
  void OnUserRemoved(const AccountId& account_id,
                     user_manager::UserRemovalReason reason) override;

  // ProfileObserver
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // chromeos::PowerManagerClient::Observer
  void SuspendDone(base::TimeDelta sleep_duration) override;

  void OnAuthSuccess(const ash::UserContext& user_context) override {}

  void OnAuthFailure(const ash::AuthFailure& error) override;

  void OnAuthAttemptStarted() override;

  void OnSessionWillBeTerminated() override;

 private:
  bool is_session_locked_;

  base::Clock* clock_;

  base::ObserverList<Observer> observers_;

  session_manager::SessionManager* const session_manager_;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      profile_observations_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_observation_{this};
  base::ScopedObservation<
      ash::UserSessionManager,
      ash::UserAuthenticatorObserver,
      &ash::UserSessionManager::AddUserAuthenticatorObserver,
      &ash::UserSessionManager::RemoveUserAuthenticatorObserver>
      authenticator_observation_{this};

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observation_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_MANAGED_SESSION_SERVICE_H_
