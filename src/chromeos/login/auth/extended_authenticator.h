// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_H_
#define CHROMEOS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"

namespace chromeos {

class AuthStatusConsumer;
class UserContext;

// An interface to interact with cryptohomed: mount home dirs, create new home
// dirs, update passwords.
//
// Typical flow:
// AuthenticateToMount() calls cryptohomed to perform offline login,
// AuthenticateToCreate() calls cryptohomed to create new cryptohome.
class COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) ExtendedAuthenticator
    : public base::RefCountedThreadSafe<ExtendedAuthenticator> {
 public:
  enum AuthState {
    SUCCESS,       // Login succeeded.
    NO_MOUNT,      // No cryptohome exist for user.
    FAILED_MOUNT,  // Failed to mount existing cryptohome - login failed.
    FAILED_TPM,    // Failed to mount/create cryptohome because of TPM error.
  };

  using ResultCallback = base::OnceCallback<void(const std::string& result)>;
  using ContextCallback = base::OnceCallback<void(const UserContext& context)>;

  static scoped_refptr<ExtendedAuthenticator> Create(
      AuthStatusConsumer* consumer);

  // Updates consumer of the class.
  virtual void SetConsumer(AuthStatusConsumer* consumer) = 0;

  // This call will attempt to authenticate the user with the key (and key
  // label) in |context|. No further actions are taken after authentication.
  virtual void AuthenticateToCheck(const UserContext& context,
                                   base::OnceClosure success_callback) = 0;

  // Attempts to start fingerprint auth session (prepare biometrics daemon for
  // upcoming fingerprint scan) for the user with |account_id|. |callback| will
  // be invoked with whether the fingerprint auth session is successfully
  // started.
  virtual void StartFingerprintAuthSession(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) = 0;

  // Attempts to end the current fingerprint auth session. Logs an error if no
  // response.
  virtual void EndFingerprintAuthSession() = 0;

  // Waits for a fingerprint scan from the user in |context|, and calls
  // |callback| with a fingerprint-specific CryptohomeErrorCode. No further
  // actions are taken after authentication.
  virtual void AuthenticateWithFingerprint(
      const UserContext& context,
      base::OnceCallback<void(user_data_auth::CryptohomeErrorCode)>
          callback) = 0;

  // Attempts to add a new |key| for the user identified/authorized by
  // |context|. If a key with the same label already exists, the behavior
  // depends on the |replace_existing| flag. If the flag is set, the old key is
  // replaced. If the flag is not set, an error occurs. It is not allowed to
  // replace the key used for authorization.
  virtual void AddKey(const UserContext& context,
                      const cryptohome::KeyDefinition& key,
                      bool replace_existing,
                      base::OnceClosure success_callback) = 0;

  // Attempts to remove the key labeled |key_to_remove| for the user identified/
  // authorized by |context|. It is possible to remove the key used for
  // authorization, although it should be done with extreme care.
  virtual void RemoveKey(const UserContext& context,
                         const std::string& key_to_remove,
                         base::OnceClosure success_callback) = 0;

  // Hashes the key in |user_context| with the system salt it its type is
  // KEY_TYPE_PASSWORD_PLAIN and passes the resulting UserContext to the
  // |callback|.
  virtual void TransformKeyIfNeeded(const UserContext& user_context,
                                    ContextCallback callback) = 0;

 protected:
  ExtendedAuthenticator();
  virtual ~ExtendedAuthenticator();

 private:
  friend class base::RefCountedThreadSafe<ExtendedAuthenticator>;

  DISALLOW_COPY_AND_ASSIGN(ExtendedAuthenticator);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::ExtendedAuthenticator;
}

#endif  // CHROMEOS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_H_
