// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_UTIL_ENCRYPTOR_H_
#define SYNC_UTIL_ENCRYPTOR_H_

#include <string>

namespace syncer {

class Encryptor {
 public:
  // All methods below should be thread-safe.
  virtual bool EncryptString(const std::string& plaintext,
                             std::string* ciphertext) = 0;

  virtual bool DecryptString(const std::string& ciphertext,
                             std::string* plaintext) = 0;

 protected:
  virtual ~Encryptor() {}
};

}  // namespace syncer

#endif  // SYNC_UTIL_ENCRYPTOR_H_
