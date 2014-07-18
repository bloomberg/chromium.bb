// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/cert_test_util.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "crypto/rsa_private_key.h"

namespace net {

scoped_ptr<crypto::RSAPrivateKey> ImportSensitiveKeyFromFile(
    const base::FilePath& dir,
    const std::string& key_filename,
    PK11SlotInfo* slot) {
  base::FilePath key_path = dir.AppendASCII(key_filename);
  std::string key_pkcs8;
  bool success = base::ReadFileToString(key_path, &key_pkcs8);
  if (!success) {
    LOG(ERROR) << "Failed to read file " << key_path.value();
    return scoped_ptr<crypto::RSAPrivateKey>();
  }

  const uint8* key_pkcs8_begin =
      reinterpret_cast<const uint8*>(key_pkcs8.data());
  std::vector<uint8> key_vector(key_pkcs8_begin,
                                key_pkcs8_begin + key_pkcs8.length());

  scoped_ptr<crypto::RSAPrivateKey> private_key(
      crypto::RSAPrivateKey::CreateSensitiveFromPrivateKeyInfo(slot,
                                                               key_vector));
  LOG_IF(ERROR, !private_key) << "Could not create key from file "
                              << key_path.value();
  return private_key.Pass();
}

}  // namespace net
