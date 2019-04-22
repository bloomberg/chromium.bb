// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_LOCK_TO_SINGLE_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_LOCK_TO_SINGLE_USER_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/login/login_state/login_state.h"

namespace policy {

// This class observes the LoginState and ArcSessionManager and checks if the
// device must be locked to a single user mount, if the policy forces it.
class LockToSingleUserManager final : public chromeos::LoginState::Observer,
                                      public arc::ArcSessionManager::Observer {
 public:
  LockToSingleUserManager();
  ~LockToSingleUserManager() override;

  // chromeos::LoginState::Observer overrides
  void LoggedInStateChanged() override;

  // arc::ArcSessionManager::Observer overrides
  void OnArcStarted() override;

 private:
  // Sends D-Bus request to lock device to single user mount.
  void LockToSingleUser();

  // Processes the response from D-Bus call.
  void OnLockToSingleUserMountUntilRebootDone(
      base::Optional<cryptohome::BaseReply> reply);

  ScopedObserver<arc::ArcSessionManager, arc::ArcSessionManager::Observer>
      arc_session_observer_{this};
  ScopedObserver<chromeos::LoginState, chromeos::LoginState::Observer>
      login_state_observer_{this};

  base::WeakPtrFactory<LockToSingleUserManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LockToSingleUserManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_LOCK_TO_SINGLE_USER_MANAGER_H_
