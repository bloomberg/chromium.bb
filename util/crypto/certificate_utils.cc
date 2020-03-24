// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/crypto/certificate_utils.h"

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <time.h>

#include <string>

#include "util/crypto/openssl_util.h"
#include "util/crypto/sha2.h"
#include "util/logging.h"

namespace openscreen {

namespace {

// These values are bit positions from RFC 5280 4.2.1.3 and will be passed to
// ASN1_BIT_STRING_set_bit.
enum KeyUsageBits {
  kDigitalSignature = 0,
  kKeyCertSign = 5,
};

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
    std::chrono::seconds time_since_unix_epoch,
    bool make_ca,
    X509* issuer,
    EVP_PKEY* issuer_key) {
  OSP_DCHECK((!!issuer) == (!!issuer_key));
  bssl::UniquePtr<X509> certificate(X509_new());
  if (!issuer) {
    issuer = certificate.get();
  }
  if (!issuer_key) {
    issuer_key = &key_pair;
  }

  // Serial numbers must be unique for this session. As a pretend CA, we should
  // not issue certificates with the same serial number in the same session.
  static int serial_number(1);
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

  bssl::UniquePtr<ASN1_BIT_STRING> x(ASN1_BIT_STRING_new());
  ASN1_BIT_STRING_set_bit(x.get(), KeyUsageBits::kDigitalSignature, 1);
  if (make_ca) {
    ASN1_BIT_STRING_set_bit(x.get(), KeyUsageBits::kKeyCertSign, 1);
  }
  if (X509_add1_ext_i2d(certificate.get(), NID_key_usage, x.get(), 0, 0) != 1) {
    return nullptr;
  }
  if (make_ca) {
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, issuer, certificate.get(), nullptr, nullptr, 0);
    bssl::UniquePtr<X509_EXTENSION> ex(
        X509V3_EXT_nconf_nid(nullptr, &ctx, NID_basic_constraints,
                             const_cast<char*>("critical,CA:TRUE")));
    if (!ex) {
      return nullptr;
    }
    void* thing = X509V3_EXT_d2i(ex.get());
    X509_add1_ext_i2d(certificate.get(), NID_basic_constraints, thing, 1, 0);
    X509V3_EXT_free(NID_basic_constraints, thing);
  }

  X509_NAME* issuer_name = X509_get_subject_name(issuer);
  if ((X509_set_issuer_name(certificate.get(), issuer_name) != 1) ||
      (X509_set_pubkey(certificate.get(), &key_pair) != 1) ||
      // Unlike all of the other BoringSSL methods here, X509_sign returns
      // the size of the signature in bytes.
      (X509_sign(certificate.get(), issuer_key, EVP_sha256()) <= 0) ||
      (X509_verify(certificate.get(), issuer_key) != 1)) {
    return nullptr;
  }

  return certificate;
}

}  // namespace

bssl::UniquePtr<EVP_PKEY> GenerateRsaKeyPair(int key_bits) {
  bssl::UniquePtr<BIGNUM> prime(BN_new());
  if (BN_set_word(prime.get(), RSA_F4) == 0) {
    return nullptr;
  }

  bssl::UniquePtr<RSA> rsa(RSA_new());
  if (RSA_generate_key_ex(rsa.get(), key_bits, prime.get(), nullptr) == 0) {
    return nullptr;
  }

  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  if (EVP_PKEY_set1_RSA(pkey.get(), rsa.get()) == 0) {
    return nullptr;
  }

  return pkey;
}

ErrorOr<bssl::UniquePtr<X509>> CreateSelfSignedX509Certificate(
    absl::string_view name,
    std::chrono::seconds duration,
    const EVP_PKEY& key_pair,
    std::chrono::seconds time_since_unix_epoch) {
  bssl::UniquePtr<X509> certificate = CreateCertificateInternal(
      name, duration, key_pair, time_since_unix_epoch, false, nullptr, nullptr);
  if (!certificate) {
    return Error::Code::kCertificateCreationError;
  }
  return certificate;
}

ErrorOr<bssl::UniquePtr<X509>> CreateSelfSignedX509CertificateForTest(
    absl::string_view name,
    std::chrono::seconds duration,
    const EVP_PKEY& key_pair,
    std::chrono::seconds time_since_unix_epoch,
    bool make_ca,
    X509* issuer,
    EVP_PKEY* issuer_key) {
  bssl::UniquePtr<X509> certificate =
      CreateCertificateInternal(name, duration, key_pair, time_since_unix_epoch,
                                make_ca, issuer, issuer_key);
  if (!certificate) {
    return Error::Code::kCertificateCreationError;
  }
  return certificate;
}

ErrorOr<std::vector<uint8_t>> ExportX509CertificateToDer(
    const X509& certificate) {
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

ErrorOr<bssl::UniquePtr<EVP_PKEY>> ImportRSAPrivateKey(
    const uint8_t* der_rsa_private_key,
    int key_length) {
  if (!der_rsa_private_key || key_length == 0) {
    return Error::Code::kParameterInvalid;
  }

  RSA* rsa = RSA_private_key_from_bytes(der_rsa_private_key, key_length);
  if (!rsa) {
    return Error::Code::kRSAKeyParseError;
  }
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  EVP_PKEY_assign_RSA(pkey.get(), rsa);
  return pkey;
}

std::string GetSpkiTlv(X509* cert) {
  int len = i2d_X509_PUBKEY(cert->cert_info->key, nullptr);
  if (len <= 0) {
    return {};
  }
  std::string x(len, 0);
  uint8_t* data = reinterpret_cast<uint8_t*>(&x[0]);
  if (!i2d_X509_PUBKEY(cert->cert_info->key, &data)) {
    return {};
  }
  return x;
}

ErrorOr<uint64_t> ParseDerUint64(ASN1_INTEGER* asn1int) {
  if (asn1int->length > 8 || asn1int->length == 0) {
    return Error::Code::kParameterInvalid;
  }
  uint64_t result = 0;
  for (int i = 0; i < asn1int->length; ++i) {
    result = (result << 8) | asn1int->data[i];
  }
  return result;
}

}  // namespace openscreen
