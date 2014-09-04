// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/cert_test_util.h"

#include <pk11pub.h>
#include <secmodt.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "crypto/nss_util.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/cert_type.h"

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

bool ImportClientCertToSlot(const scoped_refptr<X509Certificate>& cert,
                            PK11SlotInfo* slot) {
  std::string nickname = cert->GetDefaultNickname(USER_CERT);
  {
    crypto::AutoNSSWriteLock lock;
    SECStatus rv = PK11_ImportCert(slot,
                                   cert->os_cert_handle(),
                                   CK_INVALID_HANDLE,
                                   nickname.c_str(),
                                   PR_FALSE);
    if (rv != SECSuccess) {
      LOG(ERROR) << "Could not import cert";
      return false;
    }
  }
  return true;
}

scoped_refptr<X509Certificate> ImportClientCertAndKeyFromFile(
    const base::FilePath& dir,
    const std::string& cert_filename,
    const std::string& key_filename,
    PK11SlotInfo* slot) {
  if (!ImportSensitiveKeyFromFile(dir, key_filename, slot)) {
    LOG(ERROR) << "Could not import private key from file " << key_filename;
    return NULL;
  }

  scoped_refptr<X509Certificate> cert(ImportCertFromFile(dir, cert_filename));

  if (!cert.get()) {
    LOG(ERROR) << "Failed to parse cert from file " << cert_filename;
    return NULL;
  }

  if (!ImportClientCertToSlot(cert, slot))
    return NULL;

  // |cert| continues to point to the original X509Certificate before the
  // import to |slot|. However this should not make a difference as NSS handles
  // state globally.
  return cert;
}

}  // namespace net
