// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_platform_key_win.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/task_environment.h"
#include "crypto/scoped_capi_types.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_private_key_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

struct TestKey {
  const char* name;
  const char* cert_file;
  const char* key_file;
  int type;
};

const TestKey kTestKeys[] = {
    {"RSA", "client_1.pem", "client_1.pk8", EVP_PKEY_RSA},
    {"ECDSA_P256", "client_4.pem", "client_4.pk8", EVP_PKEY_EC},
    {"ECDSA_P384", "client_5.pem", "client_5.pk8", EVP_PKEY_EC},
    {"ECDSA_P521", "client_6.pem", "client_6.pk8", EVP_PKEY_EC},
};

std::string TestKeyToString(const testing::TestParamInfo<TestKey>& params) {
  return params.param.name;
}

class ScopedNCRYPT_PROV_HANDLE {
 public:
  ScopedNCRYPT_PROV_HANDLE(NCRYPT_PROV_HANDLE prov) : prov_(prov) {}
  ~ScopedNCRYPT_PROV_HANDLE() { NCryptFreeObject(prov_); }

 private:
  NCRYPT_PROV_HANDLE prov_;
};

// Appends |bn| to |cbb|, represented as |len| bytes in little-endian order,
// zero-padded as needed. Returns true on success and false if |len| is too
// small.
bool AddBIGNUMLittleEndian(CBB* cbb, const BIGNUM* bn, size_t len) {
  uint8_t* ptr;
  return CBB_add_space(cbb, &ptr, len) && BN_bn2le_padded(ptr, len, bn);
}

// Converts the PKCS#8 PrivateKeyInfo structure serialized in |pkcs8| to a
// private key BLOB, suitable for import with CAPI using Microsoft Base
// Cryptographic Provider.
bool PKCS8ToBLOBForCAPI(const std::string& pkcs8, std::vector<uint8_t>* blob) {
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(pkcs8.data()), pkcs8.size());
  bssl::UniquePtr<EVP_PKEY> key(EVP_parse_private_key(&cbs));
  if (!key || CBS_len(&cbs) != 0 || EVP_PKEY_id(key.get()) != EVP_PKEY_RSA)
    return false;
  const RSA* rsa = EVP_PKEY_get0_RSA(key.get());

  // See
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa375601(v=vs.85).aspx
  PUBLICKEYSTRUC header = {0};
  header.bType = PRIVATEKEYBLOB;
  header.bVersion = 2;
  header.aiKeyAlg = CALG_RSA_SIGN;

  RSAPUBKEY rsapubkey = {0};
  rsapubkey.magic = 0x32415352;
  rsapubkey.bitlen = BN_num_bits(rsa->n);
  rsapubkey.pubexp = BN_get_word(rsa->e);

  uint8_t* blob_data;
  size_t blob_len;
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), sizeof(header) + sizeof(rsapubkey) + pkcs8.size()) ||
      !CBB_add_bytes(cbb.get(), reinterpret_cast<const uint8_t*>(&header),
                     sizeof(header)) ||
      !CBB_add_bytes(cbb.get(), reinterpret_cast<const uint8_t*>(&rsapubkey),
                     sizeof(rsapubkey)) ||
      !AddBIGNUMLittleEndian(cbb.get(), rsa->n, rsapubkey.bitlen / 8) ||
      !AddBIGNUMLittleEndian(cbb.get(), rsa->p, rsapubkey.bitlen / 16) ||
      !AddBIGNUMLittleEndian(cbb.get(), rsa->q, rsapubkey.bitlen / 16) ||
      !AddBIGNUMLittleEndian(cbb.get(), rsa->dmp1, rsapubkey.bitlen / 16) ||
      !AddBIGNUMLittleEndian(cbb.get(), rsa->dmq1, rsapubkey.bitlen / 16) ||
      !AddBIGNUMLittleEndian(cbb.get(), rsa->iqmp, rsapubkey.bitlen / 16) ||
      !AddBIGNUMLittleEndian(cbb.get(), rsa->d, rsapubkey.bitlen / 8) ||
      !CBB_finish(cbb.get(), &blob_data, &blob_len)) {
    return false;
  }

  blob->assign(blob_data, blob_data + blob_len);
  OPENSSL_free(blob_data);
  return true;
}

// Appends |bn| to |cbb|, represented as |len| bytes in big-endian order,
// zero-padded as needed. Returns true on success and false if |len| is too
// small.
bool AddBIGNUMBigEndian(CBB* cbb, const BIGNUM* bn, size_t len) {
  uint8_t* ptr;
  return CBB_add_space(cbb, &ptr, len) && BN_bn2bin_padded(ptr, len, bn);
}

// Converts the PKCS#8 PrivateKeyInfo structure serialized in |pkcs8| to a
// private key BLOB, suitable for import with CNG using the Microsoft Software
// KSP, and sets |*blob_type| to the type of the BLOB.
bool PKCS8ToBLOBForCNG(const std::string& pkcs8,
                       LPCWSTR* blob_type,
                       std::vector<uint8_t>* blob) {
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(pkcs8.data()), pkcs8.size());
  bssl::UniquePtr<EVP_PKEY> key(EVP_parse_private_key(&cbs));
  if (!key || CBS_len(&cbs) != 0)
    return false;

  if (EVP_PKEY_id(key.get()) == EVP_PKEY_RSA) {
    // See
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa375531(v=vs.85).aspx.
    const RSA* rsa = EVP_PKEY_get0_RSA(key.get());
    BCRYPT_RSAKEY_BLOB header = {0};
    header.Magic = BCRYPT_RSAFULLPRIVATE_MAGIC;
    header.BitLength = BN_num_bits(rsa->n);
    header.cbPublicExp = BN_num_bytes(rsa->e);
    header.cbModulus = BN_num_bytes(rsa->n);
    header.cbPrime1 = BN_num_bytes(rsa->p);
    header.cbPrime2 = BN_num_bytes(rsa->q);

    uint8_t* blob_data;
    size_t blob_len;
    bssl::ScopedCBB cbb;
    if (!CBB_init(cbb.get(), sizeof(header) + pkcs8.size()) ||
        !CBB_add_bytes(cbb.get(), reinterpret_cast<const uint8_t*>(&header),
                       sizeof(header)) ||
        !AddBIGNUMBigEndian(cbb.get(), rsa->e, header.cbPublicExp) ||
        !AddBIGNUMBigEndian(cbb.get(), rsa->n, header.cbModulus) ||
        !AddBIGNUMBigEndian(cbb.get(), rsa->p, header.cbPrime1) ||
        !AddBIGNUMBigEndian(cbb.get(), rsa->q, header.cbPrime2) ||
        !AddBIGNUMBigEndian(cbb.get(), rsa->dmp1, header.cbPrime1) ||
        !AddBIGNUMBigEndian(cbb.get(), rsa->dmq1, header.cbPrime2) ||
        !AddBIGNUMBigEndian(cbb.get(), rsa->iqmp, header.cbPrime1) ||
        !AddBIGNUMBigEndian(cbb.get(), rsa->d, header.cbModulus) ||
        !CBB_finish(cbb.get(), &blob_data, &blob_len)) {
      return false;
    }

    *blob_type = BCRYPT_RSAFULLPRIVATE_BLOB;
    blob->assign(blob_data, blob_data + blob_len);
    OPENSSL_free(blob_data);
    return true;
  }

  if (EVP_PKEY_id(key.get()) == EVP_PKEY_EC) {
    // See
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa375520(v=vs.85).aspx.
    const EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(key.get());
    const EC_GROUP* group = EC_KEY_get0_group(ec_key);
    bssl::UniquePtr<BIGNUM> x(BN_new());
    bssl::UniquePtr<BIGNUM> y(BN_new());
    if (!EC_POINT_get_affine_coordinates_GFp(
            group, EC_KEY_get0_public_key(ec_key), x.get(), y.get(), nullptr)) {
      return false;
    }

    BCRYPT_ECCKEY_BLOB header = {0};
    switch (EC_GROUP_get_curve_name(EC_KEY_get0_group(ec_key))) {
      case NID_X9_62_prime256v1:
        header.dwMagic = BCRYPT_ECDSA_PRIVATE_P256_MAGIC;
        break;
      case NID_secp384r1:
        header.dwMagic = BCRYPT_ECDSA_PRIVATE_P384_MAGIC;
        break;
      case NID_secp521r1:
        header.dwMagic = BCRYPT_ECDSA_PRIVATE_P521_MAGIC;
        break;
      default:
        return false;
    }
    header.cbKey = BN_num_bytes(EC_GROUP_get0_order(group));

    uint8_t* blob_data;
    size_t blob_len;
    bssl::ScopedCBB cbb;
    if (!CBB_init(cbb.get(), sizeof(header) + header.cbKey * 3) ||
        !CBB_add_bytes(cbb.get(), reinterpret_cast<const uint8_t*>(&header),
                       sizeof(header)) ||
        !AddBIGNUMBigEndian(cbb.get(), x.get(), header.cbKey) ||
        !AddBIGNUMBigEndian(cbb.get(), y.get(), header.cbKey) ||
        !AddBIGNUMBigEndian(cbb.get(), EC_KEY_get0_private_key(ec_key),
                            header.cbKey) ||
        !CBB_finish(cbb.get(), &blob_data, &blob_len)) {
      return false;
    }

    *blob_type = BCRYPT_ECCPRIVATE_BLOB;
    blob->assign(blob_data, blob_data + blob_len);
    OPENSSL_free(blob_data);
    return true;
  }

  return false;
}

}  // namespace

class SSLPlatformKeyCNGTest : public testing::TestWithParam<TestKey>,
                              public WithTaskEnvironment {};

TEST_P(SSLPlatformKeyCNGTest, KeyMatches) {
  const TestKey& test_key = GetParam();

  // Load test data.
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), test_key.cert_file);
  ASSERT_TRUE(cert);

  std::string pkcs8;
  base::FilePath pkcs8_path =
      GetTestCertsDirectory().AppendASCII(test_key.key_file);
  ASSERT_TRUE(base::ReadFileToString(pkcs8_path, &pkcs8));

  // Import the key into CNG. Per MSDN's documentation on NCryptImportKey, if a
  // key name is not supplied (via the pParameterList parameter for the BLOB
  // types we use), the Microsoft Software KSP will treat the key as ephemeral.
  //
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa376276(v=vs.85).aspx
  NCRYPT_PROV_HANDLE prov;
  SECURITY_STATUS status =
      NCryptOpenStorageProvider(&prov, MS_KEY_STORAGE_PROVIDER, 0);
  ASSERT_FALSE(FAILED(status)) << status;
  ScopedNCRYPT_PROV_HANDLE scoped_prov(prov);

  LPCWSTR blob_type;
  std::vector<uint8_t> blob;
  ASSERT_TRUE(PKCS8ToBLOBForCNG(pkcs8, &blob_type, &blob));
  NCRYPT_KEY_HANDLE ncrypt_key;
  status = NCryptImportKey(prov, 0 /* hImportKey */, blob_type,
                           nullptr /* pParameterList */, &ncrypt_key,
                           blob.data(), blob.size(), NCRYPT_SILENT_FLAG);
  ASSERT_FALSE(FAILED(status)) << status;

  scoped_refptr<SSLPrivateKey> key = WrapCNGPrivateKey(cert.get(), ncrypt_key);
  ASSERT_TRUE(key);

  EXPECT_EQ(SSLPrivateKey::DefaultAlgorithmPreferences(test_key.type,
                                                       true /* supports PSS */),
            key->GetAlgorithmPreferences());

  TestSSLPrivateKeyMatches(key.get(), pkcs8);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SSLPlatformKeyCNGTest,
                         testing::ValuesIn(kTestKeys),
                         TestKeyToString);

TEST(SSLPlatformKeyCAPITest, KeyMatches) {
  base::test::TaskEnvironment task_environment;

  // Load test data.
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), "client_1.pem");
  ASSERT_TRUE(cert);

  std::string pkcs8;
  base::FilePath pkcs8_path =
      GetTestCertsDirectory().AppendASCII("client_1.pk8");
  ASSERT_TRUE(base::ReadFileToString(pkcs8_path, &pkcs8));

  // Import the key into CAPI. Use CRYPT_VERIFYCONTEXT for an ephemeral key.
  crypto::ScopedHCRYPTPROV prov;
  ASSERT_NE(FALSE,
            CryptAcquireContext(prov.receive(), nullptr, nullptr, PROV_RSA_AES,
                                CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
      << GetLastError();

  std::vector<uint8_t> blob;
  ASSERT_TRUE(PKCS8ToBLOBForCAPI(pkcs8, &blob));

  crypto::ScopedHCRYPTKEY hcryptkey;
  ASSERT_NE(FALSE, CryptImportKey(prov.get(), blob.data(), blob.size(),
                                  0 /* hPubKey */, 0 /* dwFlags */,
                                  hcryptkey.receive()))
      << GetLastError();
  // Release |hcryptkey| so it does not outlive |prov|.
  hcryptkey.reset();

  scoped_refptr<SSLPrivateKey> key =
      WrapCAPIPrivateKey(cert.get(), prov.release(), AT_SIGNATURE);
  ASSERT_TRUE(key);

  std::vector<uint16_t> expected = {
      SSL_SIGN_RSA_PKCS1_SHA1, SSL_SIGN_RSA_PKCS1_SHA256,
      SSL_SIGN_RSA_PKCS1_SHA384, SSL_SIGN_RSA_PKCS1_SHA512,
  };
  EXPECT_EQ(expected, key->GetAlgorithmPreferences());

  TestSSLPrivateKeyMatches(key.get(), pkcs8);
}

}  // namespace net
