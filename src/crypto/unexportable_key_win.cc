// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <ncrypt.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/scoped_generic.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "crypto/unexportable_key.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace crypto {

namespace {

// NCrypt has a style of returning handles by writing opaque pointers to
// caller-provided locations. These pointers must be passed to
// |NCryptFreeObject| when no longer needed.
template <typename T>
struct NCryptObjectTraits {
  // In practice a value of zero makes |NCryptFreeObject| a no-op, but this
  // isn't specified by the documentation so the code below avoids depending on
  // this by releasing() values that were never initialised.
  static T InvalidValue() { return 0; }
  static void Free(T handle) { NCryptFreeObject(handle); }
};

using ScopedProvider =
    base::ScopedGeneric<NCRYPT_PROV_HANDLE,
                        NCryptObjectTraits<NCRYPT_PROV_HANDLE>>;
using ScopedKey = base::ScopedGeneric<NCRYPT_KEY_HANDLE,
                                      NCryptObjectTraits<NCRYPT_KEY_HANDLE>>;

std::vector<uint8_t> CBBToVector(const CBB* cbb) {
  return std::vector<uint8_t>(CBB_data(cbb), CBB_data(cbb) + CBB_len(cbb));
}

// BCryptAlgorithmFor returns the BCrypt algorithm ID for the given Chromium
// signing algorithm.
absl::optional<LPCWSTR> BCryptAlgorithmFor(
    SignatureVerifier::SignatureAlgorithm algo) {
  switch (algo) {
    case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
      return BCRYPT_RSA_ALGORITHM;

    case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
      return BCRYPT_ECDSA_P256_ALGORITHM;

    default:
      return absl::nullopt;
  }
}

// GetBestSupported returns the first element of |acceptable_algorithms| that
// |provider| supports, or |nullopt| if there isn't any.
absl::optional<SignatureVerifier::SignatureAlgorithm> GetBestSupported(
    NCRYPT_PROV_HANDLE provider,
    base::span<const SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms) {
  for (auto algo : acceptable_algorithms) {
    absl::optional<LPCWSTR> bcrypto_algo_name = BCryptAlgorithmFor(algo);
    if (!bcrypto_algo_name) {
      continue;
    }

    if (!FAILED(NCryptIsAlgSupported(provider, *bcrypto_algo_name,
                                     /*flags=*/0))) {
      return algo;
    }
  }

  return absl::nullopt;
}

// GetKeyProperty returns the given NCrypt key property of |key|.
absl::optional<std::vector<uint8_t>> GetKeyProperty(NCRYPT_KEY_HANDLE key,
                                                    LPCWSTR property) {
  DWORD size;
  if (FAILED(NCryptGetProperty(key, property, nullptr, 0, &size, 0))) {
    return absl::nullopt;
  }

  std::vector<uint8_t> ret(size);
  if (FAILED(
          NCryptGetProperty(key, property, ret.data(), ret.size(), &size, 0))) {
    return absl::nullopt;
  }
  CHECK_EQ(ret.size(), size);

  return ret;
}

// ExportKey returns |key| exported in the given format or nullopt on error.
absl::optional<std::vector<uint8_t>> ExportKey(NCRYPT_KEY_HANDLE key,
                                               LPCWSTR format) {
  DWORD output_size;
  if (FAILED(NCryptExportKey(key, 0, format, nullptr, nullptr, 0, &output_size,
                             0))) {
    return absl::nullopt;
  }

  std::vector<uint8_t> output(output_size);
  if (FAILED(NCryptExportKey(key, 0, format, nullptr, output.data(),
                             output.size(), &output_size, 0))) {
    return absl::nullopt;
  }
  CHECK_EQ(output.size(), output_size);

  return output;
}

absl::optional<std::vector<uint8_t>> GetP256ECDSASPKI(NCRYPT_KEY_HANDLE key) {
  const absl::optional<std::vector<uint8_t>> pub_key =
      ExportKey(key, BCRYPT_ECCPUBLIC_BLOB);
  if (!pub_key) {
    return absl::nullopt;
  }

  // The exported key is a |BCRYPT_ECCKEY_BLOB| followed by the bytes of the
  // public key itself.
  // https://docs.microsoft.com/en-us/windows/win32/api/bcrypt/ns-bcrypt-bcrypt_ecckey_blob
  BCRYPT_ECCKEY_BLOB header;
  if (pub_key->size() < sizeof(header)) {
    return absl::nullopt;
  }
  memcpy(&header, pub_key->data(), sizeof(header));
  // |cbKey| is documented[1] as "the length, in bytes, of the key". It is
  // not. For ECDSA public keys it is the length of a field element.
  if (header.dwMagic != BCRYPT_ECDSA_PUBLIC_P256_MAGIC ||
      header.cbKey != 256 / 8 ||
      pub_key->size() - sizeof(BCRYPT_ECCKEY_BLOB) != 64) {
    return absl::nullopt;
  }

  uint8_t x962[1 + 32 + 32];
  x962[0] = POINT_CONVERSION_UNCOMPRESSED;
  memcpy(&x962[1], pub_key->data() + sizeof(BCRYPT_ECCKEY_BLOB), 64);

  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p256.get()));
  if (!EC_POINT_oct2point(p256.get(), point.get(), x962, sizeof(x962),
                          /*ctx=*/nullptr)) {
    return absl::nullopt;
  }
  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(EC_KEY_set_public_key(ec_key.get(), point.get()));
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  CHECK(EVP_PKEY_set1_EC_KEY(pkey.get(), ec_key.get()));

  bssl::ScopedCBB cbb;
  CHECK(CBB_init(cbb.get(), /*initial_capacity=*/128) &&
        EVP_marshal_public_key(cbb.get(), pkey.get()));
  return CBBToVector(cbb.get());
}

absl::optional<std::vector<uint8_t>> GetRSASPKI(NCRYPT_KEY_HANDLE key) {
  const absl::optional<std::vector<uint8_t>> pub_key =
      ExportKey(key, BCRYPT_RSAPUBLIC_BLOB);
  if (!pub_key) {
    return absl::nullopt;
  }

  // The exported key is a |BCRYPT_RSAKEY_BLOB| followed by the bytes of the
  // key itself.
  // https://docs.microsoft.com/en-us/windows/win32/api/bcrypt/ns-bcrypt-bcrypt_rsakey_blob
  BCRYPT_RSAKEY_BLOB header;
  if (pub_key->size() < sizeof(header)) {
    return absl::nullopt;
  }
  memcpy(&header, pub_key->data(), sizeof(header));
  if (header.Magic != static_cast<ULONG>(BCRYPT_RSAPUBLIC_MAGIC)) {
    return absl::nullopt;
  }

  size_t bytes_needed;
  if (!base::CheckAdd(sizeof(BCRYPT_RSAKEY_BLOB),
                      base::CheckAdd(header.cbPublicExp, header.cbModulus))
           .AssignIfValid(&bytes_needed) ||
      pub_key->size() < bytes_needed) {
    return absl::nullopt;
  }

  bssl::UniquePtr<BIGNUM> e(
      BN_bin2bn(&pub_key->data()[sizeof(BCRYPT_RSAKEY_BLOB)],
                header.cbPublicExp, nullptr));
  bssl::UniquePtr<BIGNUM> n(BN_bin2bn(
      &pub_key->data()[sizeof(BCRYPT_RSAKEY_BLOB) + header.cbPublicExp],
      header.cbModulus, nullptr));

  bssl::UniquePtr<RSA> rsa(RSA_new());
  CHECK(RSA_set0_key(rsa.get(), n.release(), e.release(), nullptr));
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  CHECK(EVP_PKEY_set1_RSA(pkey.get(), rsa.get()));

  bssl::ScopedCBB cbb;
  CHECK(CBB_init(cbb.get(), /*initial_capacity=*/384) &&
        EVP_marshal_public_key(cbb.get(), pkey.get()));
  return CBBToVector(cbb.get());
}

// ECDSAKey wraps a TPM-stored P-256 ECDSA key.
class ECDSAKey : public UnexportableSigningKey {
 public:
  ECDSAKey(ScopedProvider provider,
           ScopedKey key,
           std::vector<uint8_t> wrapped,
           std::vector<uint8_t> spki)
      : provider_(std::move(provider)),
        key_(std::move(key)),
        wrapped_(std::move(wrapped)),
        spki_(std::move(spki)) {}

  SignatureVerifier::SignatureAlgorithm Algorithm() const override {
    return SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
  }

  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override {
    return spki_;
  }

  std::vector<uint8_t> GetWrappedKey() const override { return wrapped_; }

  absl::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    std::array<uint8_t, kSHA256Length> digest = SHA256Hash(data);
    // The signature is written as a pair of big-endian field elements for P-256
    // ECDSA.
    std::vector<uint8_t> sig(64);
    DWORD sig_size;
    if (FAILED(NCryptSignHash(key_.get(), nullptr, digest.data(), digest.size(),
                              sig.data(), sig.size(), &sig_size,
                              NCRYPT_SILENT_FLAG))) {
      return absl::nullopt;
    }
    CHECK_EQ(sig.size(), sig_size);

    bssl::UniquePtr<BIGNUM> r(BN_bin2bn(sig.data(), 32, nullptr));
    bssl::UniquePtr<BIGNUM> s(BN_bin2bn(sig.data() + 32, 32, nullptr));
    ECDSA_SIG sig_st;
    sig_st.r = r.get();
    sig_st.s = s.get();

    bssl::ScopedCBB cbb;
    CHECK(CBB_init(cbb.get(), /*initial_capacity=*/72) &&
          ECDSA_SIG_marshal(cbb.get(), &sig_st));
    return CBBToVector(cbb.get());
  }

 private:
  ScopedProvider provider_;
  ScopedKey key_;
  const std::vector<uint8_t> wrapped_;
  const std::vector<uint8_t> spki_;
};

// RSAKey wraps a TPM-stored RSA key.
class RSAKey : public UnexportableSigningKey {
 public:
  RSAKey(ScopedProvider provider,
         ScopedKey key,
         std::vector<uint8_t> wrapped,
         std::vector<uint8_t> spki)
      : provider_(std::move(provider)),
        key_(std::move(key)),
        wrapped_(std::move(wrapped)),
        spki_(std::move(spki)) {}

  SignatureVerifier::SignatureAlgorithm Algorithm() const override {
    return SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256;
  }

  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override {
    return spki_;
  }

  std::vector<uint8_t> GetWrappedKey() const override { return wrapped_; }

  absl::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    std::array<uint8_t, kSHA256Length> digest = SHA256Hash(data);
    BCRYPT_PKCS1_PADDING_INFO padding_info = {0};
    padding_info.pszAlgId = NCRYPT_SHA256_ALGORITHM;

    DWORD sig_size;
    if (FAILED(NCryptSignHash(key_.get(), &padding_info, digest.data(),
                              digest.size(), nullptr, 0, &sig_size,
                              NCRYPT_SILENT_FLAG | BCRYPT_PAD_PKCS1))) {
      return absl::nullopt;
    }

    std::vector<uint8_t> sig(sig_size);
    if (FAILED(NCryptSignHash(key_.get(), &padding_info, digest.data(),
                              digest.size(), sig.data(), sig.size(), &sig_size,
                              NCRYPT_SILENT_FLAG | BCRYPT_PAD_PKCS1))) {
      return absl::nullopt;
    }
    CHECK_EQ(sig.size(), sig_size);

    return sig;
  }

 private:
  ScopedProvider provider_;
  ScopedKey key_;
  const std::vector<uint8_t> wrapped_;
  const std::vector<uint8_t> spki_;
};

// UnexportableKeyProviderWin uses NCrypt and the Platform Crypto
// Provider to expose TPM-backed keys on Windows.
class UnexportableKeyProviderWin : public UnexportableKeyProvider {
 public:
  ~UnexportableKeyProviderWin() override = default;

  absl::optional<SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    ScopedProvider provider;
    if (FAILED(NCryptOpenStorageProvider(
            ScopedProvider::Receiver(provider).get(),
            MS_PLATFORM_CRYPTO_PROVIDER, /*flags=*/0))) {
      // If the operation failed then |provider| doesn't have a valid handle in
      // it and we shouldn't try to free it.
      (void)provider.release();
      return absl::nullopt;
    }

    return GetBestSupported(provider.get(), acceptable_algorithms);
  }

  std::unique_ptr<UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    ScopedProvider provider;
    if (FAILED(NCryptOpenStorageProvider(
            ScopedProvider::Receiver(provider).get(),
            MS_PLATFORM_CRYPTO_PROVIDER, /*flags=*/0))) {
      // If the operation failed when |provider| doesn't have a valid handle in
      // it and we shouldn't try to free it.
      (void)provider.release();
      return nullptr;
    }

    absl::optional<SignatureVerifier::SignatureAlgorithm> algo =
        GetBestSupported(provider.get(), acceptable_algorithms);
    if (!algo) {
      return nullptr;
    }

    ScopedKey key;
    // An empty key name stops the key being persisted to disk.
    if (FAILED(NCryptCreatePersistedKey(
            provider.get(), ScopedKey::Receiver(key).get(),
            BCryptAlgorithmFor(*algo).value(), /*pszKeyName=*/nullptr,
            /*dwLegacyKeySpec=*/0, /*dwFlags=*/0))) {
      // If the operation failed then |key| doesn't have a valid handle in it
      // and we shouldn't try and free it.
      (void)key.release();
      return nullptr;
    }

    if (FAILED(NCryptFinalizeKey(key.get(), NCRYPT_SILENT_FLAG))) {
      return nullptr;
    }

    const absl::optional<std::vector<uint8_t>> wrapped_key =
        ExportKey(key.get(), BCRYPT_OPAQUE_KEY_BLOB);
    if (!wrapped_key) {
      return nullptr;
    }

    absl::optional<std::vector<uint8_t>> spki;
    switch (*algo) {
      case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
        spki = GetP256ECDSASPKI(key.get());
        if (!spki) {
          return nullptr;
        }
        return std::make_unique<ECDSAKey>(std::move(provider), std::move(key),
                                          std::move(*wrapped_key),
                                          std::move(spki.value()));
      case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
        spki = GetRSASPKI(key.get());
        if (!spki) {
          return nullptr;
        }
        return std::make_unique<RSAKey>(std::move(provider), std::move(key),
                                        std::move(*wrapped_key),
                                        std::move(spki.value()));
      default:
        return nullptr;
    }
  }

  std::unique_ptr<UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    ScopedProvider provider;
    if (FAILED(NCryptOpenStorageProvider(
            ScopedProvider::Receiver(provider).get(),
            MS_PLATFORM_CRYPTO_PROVIDER, /*flags=*/0))) {
      // If the operation failed when |provider| doesn't have a valid handle in
      // it and we shouldn't try to free it.
      (void)provider.release();
      return nullptr;
    }

    ScopedKey key;
    if (FAILED(NCryptImportKey(
            provider.get(), /*hImportKey=*/NULL, BCRYPT_OPAQUE_KEY_BLOB,
            /*pParameterList=*/nullptr, ScopedKey::Receiver(key).get(),
            const_cast<PBYTE>(wrapped.data()), wrapped.size(),
            /*dwFlags=*/NCRYPT_SILENT_FLAG))) {
      // If the operation failed then |key| doesn't have a valid handle in it
      // and we shouldn't try and free it.
      (void)key.release();
      return nullptr;
    }

    const absl::optional<std::vector<uint8_t>> algo_bytes =
        GetKeyProperty(key.get(), NCRYPT_ALGORITHM_PROPERTY);
    if (!algo_bytes) {
      return nullptr;
    }

    // The documentation suggests that |NCRYPT_ALGORITHM_PROPERTY| should return
    // the original algorithm, i.e. |BCRYPT_ECDSA_P256_ALGORITHM| for ECDSA. But
    // it actually returns just "ECDSA" for that case.
    static const wchar_t kECDSA[] = L"ECDSA";
    static const wchar_t kRSA[] = BCRYPT_RSA_ALGORITHM;

    absl::optional<std::vector<uint8_t>> spki;
    if (algo_bytes->size() == sizeof(kECDSA) &&
        memcmp(algo_bytes->data(), kECDSA, sizeof(kECDSA)) == 0) {
      spki = GetP256ECDSASPKI(key.get());
      if (!spki) {
        return nullptr;
      }
      return std::make_unique<ECDSAKey>(
          std::move(provider), std::move(key),
          std::vector<uint8_t>(wrapped.begin(), wrapped.end()),
          std::move(spki.value()));
    } else if (algo_bytes->size() == sizeof(kRSA) &&
               memcmp(algo_bytes->data(), kRSA, sizeof(kRSA)) == 0) {
      spki = GetRSASPKI(key.get());
      if (!spki) {
        return nullptr;
      }
      return std::make_unique<RSAKey>(
          std::move(provider), std::move(key),
          std::vector<uint8_t>(wrapped.begin(), wrapped.end()),
          std::move(spki.value()));
    }

    return nullptr;
  }
};

}  // namespace

std::unique_ptr<UnexportableKeyProvider> GetUnexportableKeyProviderWin() {
  return std::make_unique<UnexportableKeyProviderWin>();
}

}  // namespace crypto
