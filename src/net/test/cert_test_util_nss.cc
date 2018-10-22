// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/cert_test_util.h"

#include <pk11pub.h>
#include <secmodt.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "crypto/nss_key_util.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/cert_type.h"
#include "net/cert/x509_util_nss.h"

namespace net {

bool ImportSensitiveKeyFromFile(const base::FilePath& dir,
                                const std::string& key_filename,
                                PK11SlotInfo* slot) {
  base::FilePath key_path = dir.AppendASCII(key_filename);
  std::string key_pkcs8;
  bool success = base::ReadFileToString(key_path, &key_pkcs8);
  if (!success) {
    LOG(ERROR) << "Failed to read file " << key_path.value();
    return false;
  }

  const uint8_t* key_pkcs8_begin =
      reinterpret_cast<const uint8_t*>(key_pkcs8.data());
  std::vector<uint8_t> key_vector(key_pkcs8_begin,
                                  key_pkcs8_begin + key_pkcs8.length());

  crypto::ScopedSECKEYPrivateKey private_key(
      crypto::ImportNSSKeyFromPrivateKeyInfo(slot, key_vector,
                                             true /* permanent */));
  LOG_IF(ERROR, !private_key) << "Could not create key from file "
                              << key_path.value();
  return !!private_key;
}

bool ImportClientCertToSlot(CERTCertificate* nss_cert, PK11SlotInfo* slot) {
  std::string nickname =
      x509_util::GetDefaultUniqueNickname(nss_cert, USER_CERT, slot);
  SECStatus rv = PK11_ImportCert(slot, nss_cert, CK_INVALID_HANDLE,
                                 nickname.c_str(), PR_FALSE);
  if (rv != SECSuccess) {
    LOG(ERROR) << "Could not import cert";
    return false;
  }
  return true;
}

ScopedCERTCertificate ImportClientCertToSlot(
    const scoped_refptr<X509Certificate>& cert,
    PK11SlotInfo* slot) {
  ScopedCERTCertificate nss_cert =
      x509_util::CreateCERTCertificateFromX509Certificate(cert.get());
  if (!nss_cert)
    return nullptr;

  if (!ImportClientCertToSlot(nss_cert.get(), slot))
    return nullptr;

  return nss_cert;
}

scoped_refptr<X509Certificate> ImportClientCertAndKeyFromFile(
    const base::FilePath& dir,
    const std::string& cert_filename,
    const std::string& key_filename,
    PK11SlotInfo* slot,
    ScopedCERTCertificate* nss_cert) {
  if (!ImportSensitiveKeyFromFile(dir, key_filename, slot)) {
    LOG(ERROR) << "Could not import private key from file " << key_filename;
    return NULL;
  }

  scoped_refptr<X509Certificate> cert(ImportCertFromFile(dir, cert_filename));

  if (!cert.get()) {
    LOG(ERROR) << "Failed to parse cert from file " << cert_filename;
    return NULL;
  }

  *nss_cert = ImportClientCertToSlot(cert, slot);
  if (!*nss_cert)
    return NULL;

  // |cert| continues to point to the original X509Certificate before the
  // import to |slot|. However this should not make a difference as NSS handles
  // state globally.
  return cert;
}

scoped_refptr<X509Certificate> ImportClientCertAndKeyFromFile(
    const base::FilePath& dir,
    const std::string& cert_filename,
    const std::string& key_filename,
    PK11SlotInfo* slot) {
  ScopedCERTCertificate nss_cert;
  return ImportClientCertAndKeyFromFile(dir, cert_filename, key_filename, slot,
                                        &nss_cert);
}

ScopedCERTCertificate ImportCERTCertificateFromFile(
    const base::FilePath& certs_dir,
    const std::string& cert_file) {
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, cert_file);
  if (!cert)
    return nullptr;
  return x509_util::CreateCERTCertificateFromX509Certificate(cert.get());
}

ScopedCERTCertificateList CreateCERTCertificateListFromFile(
    const base::FilePath& certs_dir,
    const std::string& cert_file,
    int format) {
  CertificateList certs =
      CreateCertificateListFromFile(certs_dir, cert_file, format);
  ScopedCERTCertificateList nss_certs;
  for (const auto& cert : certs) {
    ScopedCERTCertificate nss_cert =
        x509_util::CreateCERTCertificateFromX509Certificate(cert.get());
    if (!nss_cert)
      return {};
    nss_certs.push_back(std::move(nss_cert));
  }
  return nss_certs;
}

}  // namespace net
