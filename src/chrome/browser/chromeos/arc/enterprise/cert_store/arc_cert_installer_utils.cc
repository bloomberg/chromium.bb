// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/enterprise/cert_store/arc_cert_installer_utils.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pkcs8.h"

namespace arc {

std::string CreatePkcs12ForKey(const std::string& name, EVP_PKEY* key) {
  if (!key) {
    VLOG(1) << "Failed due to a nullptr key";
    return "";
  }

  // Make a PKCS#12 blob.
  bssl::UniquePtr<PKCS12> pkcs12(PKCS12_create(
      nullptr, name.c_str(), key, nullptr, nullptr, 0, 0, 0, 0, 0));
  if (!pkcs12) {
    VLOG(1) << "Failed to create PKCS12 object from |key| for " << name;
    return "";
  }

  uint8_t* pkcs12_key = nullptr;
  int pkcs12_key_len;
  if (!(pkcs12_key_len = i2d_PKCS12(pkcs12.get(), &pkcs12_key))) {
    VLOG(1) << "Failed to translate PKCS12 to byte array for " << name;
    return "";
  }

  bssl::UniquePtr<uint8_t> free_pkcs12_key(pkcs12_key);
  std::string encoded_pkcs12_key;
  base::Base64Encode(base::StringPiece((char*)pkcs12_key, pkcs12_key_len),
                     &encoded_pkcs12_key);
  return encoded_pkcs12_key;
}

}  // namespace arc
