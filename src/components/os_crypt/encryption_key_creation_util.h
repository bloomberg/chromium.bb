// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ENCRYPTION_KEY_CREATION_UTIL_H_
#define COMPONENTS_OS_CRYPT_ENCRYPTION_KEY_CREATION_UTIL_H_

#include "base/component_export.h"

namespace crypto {
class AppleKeychain;
}

namespace os_crypt {

// An interface for the utility that logs statistics on the encryption key on
// Mac.
// This class is used on Mac and iOS, but does nothing on iOS. The object for
// the Mac class has to be created on the main thread.
class EncryptionKeyCreationUtil {
 public:
  // The action that is taken by KeychainPassword::GetPassword method.
  // This enum is used for reporting metrics.
  enum class GetKeyAction {
    // Key was found in the Keychain and the preference that it was created in
    // the past is set.
    kKeyFound = 0,
    // Key was found in the Keychain, but the preference that it was created in
    // the past was not set.
    kKeyFoundFirstTime = 1,
    kOverwritingPrevented_OBSOLETE = 2,
    // Key was added to the Keychain and the preference is set.
    kNewKeyAddedToKeychain = 3,
    // Some other error occurred during lookup.
    kKeychainLookupFailed = 4,
    // The preference was set but a new key was added to the Keychain.
    kKeyPotentiallyOverwritten = 5,
    // The preference was set but a new key was not created due to an error.
    kKeyOverwriteFailed = 6,
    // A new key should be created but an error occured.
    kNewKeyAddError = 7,
    kMaxValue = kNewKeyAddError,
  };

  // Result of FindGenericPassword. This enum is used for reporting metrics.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FindPasswordResult {
    kOtherError = 0,
    kFound = 1,
    kNotFound = 2,
    kMaxValue = kNotFound,
  };

  virtual ~EncryptionKeyCreationUtil() = default;

  // This method is called when the encryption key is successfully retrieved
  // from the Keychain. If this is called for the very first time, it
  // asynchronously updates the preference on the main thread that the key was
  // created. This method doesn't need to be called on the main thread.
  virtual void OnKeyWasFound() = 0;

  // Called when the encryption key was not in the Keychain just before a new
  // key is stored. This method doesn't need to be called on the main thread.
  virtual void OnKeyNotFound(const crypto::AppleKeychain& keychain) = 0;

  // Called when the encryption key was not in the Keychain. |new_key_stored|
  // is true iff a new key was stored successfully. This method doesn't need to
  // be called on the main thread.
  virtual void OnKeyStored(bool new_key_stored) = 0;

  // This method is called when the Keychain returns error other than
  // errSecItemNotFound (e.g., user is not authorized to use Keychain, or
  // Keychain is unavailable for some other reasons).
  virtual void OnKeychainLookupFailed(int error) = 0;
};

}  // namespace os_crypt

#endif  // COMPONENTS_OS_CRYPT_ENCRYPTION_KEY_CREATION_UTIL_H_
