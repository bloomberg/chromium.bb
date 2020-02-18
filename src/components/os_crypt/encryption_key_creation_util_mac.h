// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ENCRYPTION_KEY_CREATION_UTIL_MAC_H_
#define COMPONENTS_OS_CRYPT_ENCRYPTION_KEY_CREATION_UTIL_MAC_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "components/os_crypt/encryption_key_creation_util.h"

class PrefService;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace os_crypt {

// A utility class which provides a method to check whether the encryption key
// should be available in the Keychain (meaning it was created in the past).
class COMPONENT_EXPORT(OS_CRYPT) EncryptionKeyCreationUtilMac
    : public EncryptionKeyCreationUtil {
 public:
  // This class has to be initialized on the main UI thread since it uses
  // the local state.
  EncryptionKeyCreationUtilMac(
      PrefService* local_state,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  ~EncryptionKeyCreationUtilMac() override;

  // os_crypt::EncryptionKeyCreationUtil:
  void OnKeyWasFound() override;
  void OnKeyNotFound(const crypto::AppleKeychain& keychain) override;
  void OnKeyStored(bool new_key_stored) override;
  void OnKeychainLookupFailed(int error) override;

 private:
  // Asynchronously updates the preference on the main thread that the
  // encryption key was created.
  void UpdateKeyCreationPreference();

  PrefService* local_state_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  volatile bool key_already_created_;

  DISALLOW_COPY_AND_ASSIGN(EncryptionKeyCreationUtilMac);
};

}  // namespace os_crypt

#endif  // COMPONENTS_OS_CRYPT_ENCRYPTION_KEY_CREATION_UTIL_MAC_H_
