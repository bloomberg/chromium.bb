// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/tls_credentials.h"

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <time.h>

#include <atomic>
#include <chrono>
#include <utility>

#include "absl/strings/str_cat.h"
#include "platform/api/logging.h"
#include "util/crypto/openssl_util.h"

namespace openscreen {
namespace platform {

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

bssl::UniquePtr<ASN1_TIME> ToAsn1Time(const Clock::time_point time) {
  // We don't have access to system_clock::to_time_t.
  const std::time_t timestamp_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch())
          .count();

  return bssl::UniquePtr<ASN1_TIME>(ASN1_TIME_set(nullptr, timestamp_seconds));
}

bssl::UniquePtr<X509> CreateCertificate(
    absl::string_view name,
    std::chrono::seconds certificate_duration,
    EVP_PKEY key_pair,
    const Clock::time_point now_time_point) {
  bssl::UniquePtr<X509> certificate(X509_new());

  // Serial numbers must be unique for this session. As a pretend CA, we should
  // not issue certificates with the same serial number in the same session.
  static std::atomic_int serial_number(1);
  if (ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()),
                       serial_number++) != 1) {
    return nullptr;
  }

  const bssl::UniquePtr<ASN1_TIME> now(ToAsn1Time(now_time_point));
  const bssl::UniquePtr<ASN1_TIME> expiration_time(
      ToAsn1Time(now_time_point + certificate_duration));

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

ErrorOr<std::vector<uint8_t>> Sha256Digest(const std::vector<uint8_t>& data) {
  bssl::UniquePtr<EVP_MD_CTX> context(EVP_MD_CTX_new());
  uint8_t digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len;
  if (!EVP_Digest(data.data(), data.size(), digest, &digest_len, EVP_sha256(),
                  nullptr)) {
    return Error::Code::kSha256HashFailure;
  }

  return std::vector<uint8_t>(digest, digest + digest_len);
}

ErrorOr<std::vector<uint8_t>> WriteOutKey(EVP_PKEY key, bool is_public) {
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_mem()));

  if (is_public) {
    if (!PEM_write_bio_PUBKEY(bio.get(), &key)) {
      return Error::Code::kParseError;
    }
  } else {
    if (!PEM_write_bio_PrivateKey(
            bio.get(), &key, nullptr /* EVP_CIPHER */, nullptr /* kstr */,
            -1 /* klen */, nullptr /* pem_password_cb */, nullptr /* u */)) {
      return Error::Code::kParseError;
    }
  }

  char* data = nullptr;
  const int len = BIO_get_mem_data(bio.get(), &data);
  if (!data || len < 0) {
    return Error::Code::kParseError;
  }

  return std::vector<uint8_t>(data, data + len);
}

}  // namespace

ErrorOr<TlsCredentials> TlsCredentials::Create(
    absl::string_view name,
    std::chrono::seconds certificate_duration,
    ClockNowFunctionPtr now_function,
    bssl::UniquePtr<EVP_PKEY> key_pair) {
  EnsureOpenSSLInit();

  bssl::UniquePtr<X509> certificate =
      CreateCertificate(name, certificate_duration, *key_pair, now_function());

  if (!certificate) {
    return Error::Code::kCertificateCreationError;
  }

  unsigned char* buffer = nullptr;
  const int len = i2d_X509(certificate.get(), &buffer);
  if (len <= 0) {
    return Error::Code::kCertificateValidationError;
  }

  std::vector<uint8_t> raw_der_certificate(buffer, buffer + len);
  // BoringSSL doesn't free the temporary buffer.
  OPENSSL_free(buffer);

  ErrorOr<std::vector<uint8_t>> private_key_or_error(
      WriteOutKey(*key_pair, false /* is_public */));
  if (!private_key_or_error) {
    return private_key_or_error.error();
  }

  ErrorOr<std::vector<uint8_t>> public_key_or_error(
      WriteOutKey(*key_pair, true /* is_public */));

  if (!public_key_or_error) {
    return public_key_or_error.error();
  }

  ErrorOr<std::vector<uint8_t>> public_key_hash_or_error(
      Sha256Digest(public_key_or_error.value()));
  if (!public_key_hash_or_error) {
    return public_key_hash_or_error.error();
  }

  return TlsCredentials(std::move(certificate), std::move(key_pair),
                        std::move(private_key_or_error.value()),
                        std::move(public_key_or_error.value()),
                        std::move(public_key_hash_or_error.value()),
                        std::move(raw_der_certificate));
}

TlsCredentials::TlsCredentials(bssl::UniquePtr<X509> certificate,
                               bssl::UniquePtr<EVP_PKEY> key_pair,
                               std::vector<uint8_t> private_key_base64,
                               std::vector<uint8_t> public_key_base64,
                               std::vector<uint8_t> public_key_hash,
                               std::vector<uint8_t> raw_der_certificate)
    : certificate_(std::move(certificate)),
      key_pair_(std::move(key_pair)),
      private_key_base64_(std::move(private_key_base64)),
      public_key_base64_(std::move(public_key_base64)),
      public_key_hash_(std::move(public_key_hash)),
      raw_der_certificate_(std::move(raw_der_certificate)) {}

}  // namespace platform
}  // namespace openscreen
