// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_AUTH_ATTEMPT_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_AUTH_ATTEMPT_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "components/account_id/account_id.h"

namespace chromeos {

// Class responsible for handling easy unlock auth attempts (both for unlocking
// the screen and logging in). The auth protocol is started by calling |Start|,
// which notifies the easy unlock app about auth attempt. When the auth result
// is available, |FinalizeUnlock| or |FinalizeSignin| should be called,
// depending on auth type.
// To cancel the in progress auth attempt, delete the |EasyUnlockAuthAttempt|
// object.
class EasyUnlockAuthAttempt {
 public:
  // The auth type.
  enum Type { TYPE_UNLOCK, TYPE_SIGNIN };

  EasyUnlockAuthAttempt(const AccountId& account_id, Type type);
  ~EasyUnlockAuthAttempt();

  // Starts the auth attempt by sending screenlockPrivate.onAuthAttempted event
  // to easy unlock app. Returns whether the event was successfully dispatched.
  bool Start();

  // Finalizes an unlock attempt. It unlocks the screen if |success| is true.
  // If |this| has TYPE_SIGNIN type, calling this method will cause signin
  // failure equivalent to cancelling the attempt.
  void FinalizeUnlock(const AccountId& account_id, bool success);

  // Finalizes signin attempt. It tries to log in using the secret derived from
  // |wrapped_secret| decrypted by |session_key|. If the decryption fails, it
  // fails the signin attempt.
  // If called on an object with TYPE_UNLOCK type, it will cause unlock failure
  // equivalent to cancelling the request.
  void FinalizeSignin(const AccountId& account_id,
                      const std::string& wrapped_secret,
                      const std::string& session_key);

 private:
  // The internal attempt state.
  enum State { STATE_IDLE, STATE_RUNNING, STATE_DONE };

  // Cancels the attempt.
  void Cancel(const AccountId& account_id);

  State state_;
  const AccountId account_id_;
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockAuthAttempt);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_AUTH_ATTEMPT_H_
