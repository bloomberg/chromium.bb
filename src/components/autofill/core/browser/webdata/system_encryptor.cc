// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/system_encryptor.h"

#include "components/os_crypt/os_crypt.h"

namespace autofill {

bool SystemEncryptor::EncryptString16(const base::string16& plaintext,
                                      std::string* ciphertext) const {
  return ::OSCrypt::EncryptString16(plaintext, ciphertext);
}

bool SystemEncryptor::DecryptString16(const std::string& ciphertext,
                                      base::string16* plaintext) const {
  return ::OSCrypt::DecryptString16(ciphertext, plaintext);
}

}  // namespace autofill
