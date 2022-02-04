// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_AUTH_TOKEN_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_AUTH_TOKEN_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class UserContext;

namespace quick_unlock {

// Security token with an identifyier string and a predetermined life time,
// after which it is irrevocably "reset" and can be considered invalid. In
// particular this makes the identifier string inaccessible from outside the
// class.
class AuthToken {
 public:
  // How long the token lives.
  static const int kTokenExpirationSeconds;

  explicit AuthToken(const UserContext& user_context);

  AuthToken(const AuthToken&) = delete;
  AuthToken& operator=(const AuthToken&) = delete;

  ~AuthToken();

  // An unguessable identifier that can be passed to webui to verify the token
  // instance has not changed. Returns nullopt if Reset() was called.
  absl::optional<std::string> Identifier() const;

  // Time since token was created or `absl::nullopt` if Reset() was called.
  absl::optional<base::TimeDelta> GetAge() const;

  // The UserContext returned here can be null if Reset() was called.
  const UserContext* user_context() const { return user_context_.get(); }

 private:
  friend class QuickUnlockStorageUnitTest;

  // Expires the token. In particular this makes the identifier string
  // inaccessible from outside the class.
  void Reset();

  base::UnguessableToken identifier_;
  base::TimeTicks creation_time_;
  std::unique_ptr<UserContext> user_context_;

  base::WeakPtrFactory<AuthToken> weak_factory_{this};
};

}  // namespace quick_unlock
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace quick_unlock {
using ::ash::quick_unlock::AuthToken;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_AUTH_TOKEN_H_
