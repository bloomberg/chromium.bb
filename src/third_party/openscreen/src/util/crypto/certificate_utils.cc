// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/crypto/certificate_utils.h"

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <time.h>

#include <atomic>
#include <string>

#include "util/crypto/openssl_util.h"
#include "util/crypto/sha2.h"

namespace openscreen {

namespace {

// Returns whether or not the certificate field successfully was added.
bool AddCertificateField(X509_NAME* certificate_name,
                         absl::string_view field,
                         absl::string_view value) {
  return X509_NAME_add_entry_by_txt(
             certificate_name, std::string(field).c_str(), MBSTRING_ASC,
             reinterpret_cast<const unsigned char*>(value.data()),
             value.length(), -1, 0) == 1;
}

bssl::UniquePtr<ASN1_TIME> ToAsn1Time(std::chrono::seconds time_since_epoch) {
  return bssl::UniquePtr<ASN1_TIME>(
      ASN1_TIME_set(nullptr, time_since_epoch.count()));
}

bssl::UniquePtr<X509> CreateCertificateInternal(
    absl::string_view name,
    std::chrono::seconds certificate_duration,
    EVP_PKEY key_pair,
    std::chrono::seconds time_since_unix_epoch) {
  bssl::UniquePtr<X509> certificate(X509_new());

  // Serial numbers must be unique for this session. As a pretend CA, we should
  // not issue certificates with the same serial number in the same session.
  static std::atomic_int serial_number(1);
  if (ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()),
                       serial_number++) != 1) {
    return nullptr;
  }

  const bssl::UniquePtr<ASN1_TIME> now(ToAsn1Time(time_since_unix_epoch));
  const bssl::UniquePtr<ASN1_TIME> expiration_time(
      ToAsn1Time(time_since_unix_epoch + certificate_duration));

  if ((X509_set_notBefore(certificate.get(), now.get()) != 1) ||
      (X509_set_notAfter(certificate.get(), expiration_time.get()) != 1)) {
    return nullptr;
  }

  X509_NAME* certificate_name = X509_get_subject_name(certificate.get());
  if (!AddCertificateField(certificate_name, "CN", name)) {
    return nullptr;
  }

  if ((X509_set_issuer_name(certificate.get(), certificate_name) != 1) ||
      (X509_set_pubkey(certificate.get(), &key_pair) != 1) ||
      // Unlike all of the other BoringSSL methods here, X509_sign returns
      // the size of the signature in bytes.
      (X509_sign(certificate.get(), &key_pair, EVP_sha256()) <= 0) ||
      (X509_verify(certificate.get(), &key_pair) != 1)) {
    return nullptr;
  }

  return certificate;
}

}  // namespace

ErrorOr<bssl::UniquePtr<X509>> CreateCertificate(
    absl::string_view name,
    std::chrono::seconds duration,
    const EVP_PKEY& key_pair,
    std::chrono::seconds time_since_unix_epoch) {
  bssl::UniquePtr<X509> certificate = CreateCertificateInternal(
      name, duration, key_pair, time_since_unix_epoch);
  if (!certificate) {
    return Error::Code::kCertificateCreationError;
  }
  return certificate;
}

ErrorOr<std::vector<uint8_t>> ExportCertificate(const X509& certificate) {
  unsigned char* buffer = nullptr;
  // Casting-away the const because the legacy i2d_X509() function is not
  // const-correct.
  X509* const certificate_ptr = const_cast<X509*>(&certificate);
  const int len = i2d_X509(certificate_ptr, &buffer);
  if (len <= 0) {
    return Error::Code::kCertificateValidationError;
  }
  std::vector<uint8_t> raw_der_certificate(buffer, buffer + len);
  // BoringSSL doesn't free the temporary buffer.
  OPENSSL_free(buffer);
  return raw_der_certificate;
}

ErrorOr<bssl::UniquePtr<X509>> ImportCertificate(const uint8_t* der_x509_cert,
                                                 int der_x509_cert_length) {
  if (!der_x509_cert) {
    return Error::Code::kErrCertsMissing;
  }
  bssl::UniquePtr<X509> certificate(
      d2i_X509(nullptr, &der_x509_cert, der_x509_cert_length));
  if (!certificate) {
    return Error::Code::kCertificateValidationError;
  }
  return certificate;
}

}  // namespace openscreen
