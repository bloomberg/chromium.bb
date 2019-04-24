// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/encryption_key_creation_util_ios.h"

namespace os_crypt {

EncryptionKeyCreationUtilIOS::EncryptionKeyCreationUtilIOS() = default;

EncryptionKeyCreationUtilIOS::~EncryptionKeyCreationUtilIOS() = default;

void EncryptionKeyCreationUtilIOS::OnKeyWasFound() {}

void EncryptionKeyCreationUtilIOS::OnKeyNotFound(
    const crypto::AppleKeychain& keychain) {}

void EncryptionKeyCreationUtilIOS::OnKeyStored(bool new_key_stored) {}

void EncryptionKeyCreationUtilIOS::OnKeychainLookupFailed(int error) {}

}  // namespace os_crypt
