// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_win.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/wincrypt_shim.h"
#include "crypto/scoped_capi_types.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/internal/test_helpers.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_win.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace {

// These tests use a series of cross-signed certificates. The overall
// hierarchy is documented in
// //net/data/ssl/scripts/generate-multi-root-test-chains.sh.
constexpr char kMultiRootCByD[] = "multi-root-C-by-D.pem";
constexpr char kMultiRootCByE[] = "multi-root-C-by-E.pem";
constexpr char kMultiRootDByD[] = "multi-root-D-by-D.pem";
constexpr char kMultiRootEByE[] = "multi-root-E-by-E.pem";
constexpr char kMultiRootFByE[] = "multi-root-F-by-E.pem";

scoped_refptr<ParsedCertificate> ParseCertFromFile(
    base::StringPiece file_name) {
  const scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(net::GetTestCertsDirectory(), file_name);
  if (!cert) {
    return nullptr;
  }
  CertErrors errors;
  scoped_refptr<ParsedCertificate> parsed = ParsedCertificate::Create(
      bssl::UpRef(cert->cert_buffer()),
      x509_util::DefaultParseCertificateOptions(), &errors);
  EXPECT_TRUE(parsed) << errors.ToDebugString();
  return parsed;
}

// Returns true if cert at file_name successfully added to store, false
// otherwise.
bool AddToStore(HCERTSTORE store, const std::string file_name) {
  const scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(net::GetTestCertsDirectory(), file_name);
  if (!cert) {
    return false;
  }
  crypto::ScopedPCCERT_CONTEXT os_cert(CertCreateCertificateContext(
      X509_ASN_ENCODING, CRYPTO_BUFFER_data(cert->cert_buffer()),
      CRYPTO_BUFFER_len(cert->cert_buffer())));
  return CertAddCertificateContextToStore(store, os_cert.get(),
                                          CERT_STORE_ADD_ALWAYS, NULL);
}

// Returns true if cert at file_name successfully added to store with
// restricted usage, false otherwise.
bool AddToStoreWithEKURestriction(HCERTSTORE store,
                                  const std::string file_name,
                                  LPCSTR usage_identifier) {
  const scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(net::GetTestCertsDirectory(), file_name);
  if (!cert) {
    return false;
  }
  crypto::ScopedPCCERT_CONTEXT os_cert(CertCreateCertificateContext(
      X509_ASN_ENCODING, CRYPTO_BUFFER_data(cert->cert_buffer()),
      CRYPTO_BUFFER_len(cert->cert_buffer())));

  CERT_ENHKEY_USAGE usage;
  memset(&usage, 0, sizeof(usage));
  if (!CertSetEnhancedKeyUsage(os_cert.get(), &usage)) {
    return false;
  }
  if (usage_identifier) {
    if (!CertAddEnhancedKeyUsageIdentifier(os_cert.get(), usage_identifier)) {
      return false;
    }
  }
  return !!CertAddCertificateContextToStore(store, os_cert.get(),
                                            CERT_STORE_ADD_ALWAYS, NULL);
}

// TrustStoreWin isset up as follows:
// Trusted as Root: D-by-D
// Known Intermediates: C-by-D, C-by-E, F-by-E
std::unique_ptr<TrustStoreWin> CreateTrustStoreWin() {
  crypto::ScopedHCERTSTORE root_store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
  crypto::ScopedHCERTSTORE intermediate_store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
  crypto::ScopedHCERTSTORE disallowed_store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));

  if (!AddToStore(root_store.get(), kMultiRootDByD) ||
      !AddToStore(intermediate_store.get(), kMultiRootCByD) ||
      !AddToStore(intermediate_store.get(), kMultiRootCByE) ||
      !AddToStore(intermediate_store.get(), kMultiRootFByE)) {
    return nullptr;
  }
  return TrustStoreWin::CreateForTesting(std::move(root_store),
                                         std::move(intermediate_store),
                                         std::move(disallowed_store));
}

TEST(TrustStoreWin, GetTrust) {
  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ASSERT_TRUE(trust_store_win);

  constexpr struct TestData {
    base::StringPiece file_name;
    CertificateTrustType expected_result;
  } kTestData[] = {
      // Explicitly trusted root should be trusted.
      {kMultiRootDByD, CertificateTrustType::TRUSTED_ANCHOR},
      // Intermediate for path building should not be trusted.
      {kMultiRootCByD, CertificateTrustType::UNSPECIFIED},
      // Unknown roots should not be trusted (e.g. just because they're
      // self-signed doesn't make them a root)
      {kMultiRootEByE, CertificateTrustType::UNSPECIFIED},
  };
  for (const auto& test_data : kTestData) {
    SCOPED_TRACE(test_data.file_name);
    auto parsed_cert = ParseCertFromFile(test_data.file_name);
    ASSERT_TRUE(parsed_cert);
    CertificateTrust trust =
        trust_store_win->GetTrust(parsed_cert.get(), nullptr);
    EXPECT_EQ(test_data.expected_result, trust.type);
  }
}

// This test has a special TrustStoreWin setup with restricted EKU usages.
// Specifically, the only certs set up in the root store are set up
// as follows:
//
// - kMultiRootDByD: only has szOID_PKIX_KP_SERVER_AUTH EKU set
// - kMultiRootEByE: only has szOID_PKIX_KP_CLIENT_AUTH set
// - kMultiRootCByD: no EKU usages set
//
// And the intermediate store as follows:
//
// - kMultiRootCByE: only has szOID_PKIX_KP_CLIENT_AUTH set
// - kMultiRootCByD: only has szOID_PKIX_KP_SERVER_AUTH EKU set
TEST(TrustStoreWin, GetTrustRestrictedEKU) {
  crypto::ScopedHCERTSTORE root_store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
  crypto::ScopedHCERTSTORE intermediate_store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
  crypto::ScopedHCERTSTORE disallowed_store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));

  ASSERT_TRUE(AddToStoreWithEKURestriction(root_store.get(), kMultiRootDByD,
                                           szOID_PKIX_KP_SERVER_AUTH));
  ASSERT_TRUE(AddToStoreWithEKURestriction(root_store.get(), kMultiRootEByE,
                                           szOID_PKIX_KP_CLIENT_AUTH));
  ASSERT_TRUE(
      AddToStoreWithEKURestriction(root_store.get(), kMultiRootCByD, nullptr));
  ASSERT_TRUE(AddToStoreWithEKURestriction(
      intermediate_store.get(), kMultiRootCByE, szOID_PKIX_KP_CLIENT_AUTH));
  ASSERT_TRUE(AddToStoreWithEKURestriction(
      intermediate_store.get(), kMultiRootCByD, szOID_PKIX_KP_SERVER_AUTH));
  std::unique_ptr<TrustStoreWin> trust_store_win =
      TrustStoreWin::CreateForTesting(std::move(root_store),
                                      std::move(intermediate_store),
                                      std::move(disallowed_store));

  constexpr struct TestData {
    base::StringPiece file_name;
    CertificateTrustType expected_result;
  } kTestData[] = {
      // Root cert with EKU szOID_PKIX_KP_SERVER_AUTH usage set should be
      // trusted.
      {kMultiRootDByD, CertificateTrustType::TRUSTED_ANCHOR},
      // Root cert with EKU szOID_PKIX_KP_CLIENT_AUTH does not allow usage of
      // cert for server auth.
      {kMultiRootEByE, CertificateTrustType::DISTRUSTED},
      // Root cert with no EKU usages but is also an intermediate cert that is
      // allowed for server auth, so we let it be used for path building.
      {kMultiRootCByD, CertificateTrustType::UNSPECIFIED},
      // Intermediate cert with EKU szOID_PKIX_KP_CLIENT_AUTH does not allow
      // usage of cert for server auth.
      {kMultiRootCByE, CertificateTrustType::DISTRUSTED},
      // Unknown cert has unspecified trust.
      {kMultiRootFByE, CertificateTrustType::UNSPECIFIED},
  };
  for (const auto& test_data : kTestData) {
    SCOPED_TRACE(test_data.file_name);
    auto parsed_cert = ParseCertFromFile(test_data.file_name);
    ASSERT_TRUE(parsed_cert);
    CertificateTrust trust =
        trust_store_win->GetTrust(parsed_cert.get(), nullptr);
    EXPECT_EQ(test_data.expected_result, trust.type);
  }
}

// Test if duplicate certs are added to the root and intermediate stores,
// possibly with different EKU usages.
TEST(TrustStoreWin, GetTrustRestrictedEKUDuplicateCerts) {
  crypto::ScopedHCERTSTORE root_store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
  crypto::ScopedHCERTSTORE intermediate_store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
  crypto::ScopedHCERTSTORE disallowed_store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));

  ASSERT_TRUE(AddToStoreWithEKURestriction(root_store.get(), kMultiRootDByD,
                                           szOID_PKIX_KP_CLIENT_AUTH));
  ASSERT_TRUE(AddToStoreWithEKURestriction(root_store.get(), kMultiRootDByD,
                                           szOID_PKIX_KP_SERVER_AUTH));
  ASSERT_TRUE(
      AddToStoreWithEKURestriction(root_store.get(), kMultiRootDByD, nullptr));
  ASSERT_TRUE(AddToStoreWithEKURestriction(
      intermediate_store.get(), kMultiRootCByD, szOID_PKIX_KP_SERVER_AUTH));
  ASSERT_TRUE(AddToStoreWithEKURestriction(
      intermediate_store.get(), kMultiRootCByD, szOID_PKIX_KP_SERVER_AUTH));
  std::unique_ptr<TrustStoreWin> trust_store_win =
      TrustStoreWin::CreateForTesting(std::move(root_store),
                                      std::move(intermediate_store),
                                      std::move(disallowed_store));

  constexpr struct TestData {
    base::StringPiece file_name;
    CertificateTrustType expected_result;
  } kTestData[] = {
      {kMultiRootDByD, CertificateTrustType::DISTRUSTED},
      // Root cert with no EKU usages but is also an intermediate cert that is
      // allowed for server auth, so we let it be used for path building.
      {kMultiRootCByD, CertificateTrustType::UNSPECIFIED},
  };
  for (const auto& test_data : kTestData) {
    SCOPED_TRACE(test_data.file_name);
    auto parsed_cert = ParseCertFromFile(test_data.file_name);
    ASSERT_TRUE(parsed_cert);
    CertificateTrust trust =
        trust_store_win->GetTrust(parsed_cert.get(), nullptr);
    EXPECT_EQ(test_data.expected_result, trust.type);
  }
}

// Test that disallowed certs with the right EKU settings will be
// distrusted.
TEST(TrustStoreWin, GetTrustDisallowedCerts) {
  crypto::ScopedHCERTSTORE root_store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
  crypto::ScopedHCERTSTORE intermediate_store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
  crypto::ScopedHCERTSTORE disallowed_store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));

  AddToStore(root_store.get(), kMultiRootDByD);
  AddToStore(root_store.get(), kMultiRootEByE);
  ASSERT_TRUE(AddToStoreWithEKURestriction(
      disallowed_store.get(), kMultiRootDByD, szOID_PKIX_KP_CLIENT_AUTH));
  AddToStore(disallowed_store.get(), kMultiRootEByE);

  std::unique_ptr<TrustStoreWin> trust_store_win =
      TrustStoreWin::CreateForTesting(std::move(root_store),
                                      std::move(intermediate_store),
                                      std::move(disallowed_store));

  constexpr struct TestData {
    base::StringPiece file_name;
    CertificateTrustType expected_result;
  } kTestData[] = {
      // dByD in root, also in distrusted but without szOID_PKIX_KP_SERVER_AUTH
      // set.
      {kMultiRootDByD, CertificateTrustType::TRUSTED_ANCHOR},
      // dByD in root, also in distrusted with szOID_PKIX_KP_SERVER_AUTH set.
      {kMultiRootEByE, CertificateTrustType::DISTRUSTED},
  };
  for (const auto& test_data : kTestData) {
    SCOPED_TRACE(test_data.file_name);
    auto parsed_cert = ParseCertFromFile(test_data.file_name);
    ASSERT_TRUE(parsed_cert);
    CertificateTrust trust =
        trust_store_win->GetTrust(parsed_cert.get(), nullptr);
    EXPECT_EQ(test_data.expected_result, trust.type);
  }
}

MATCHER_P(ParsedCertEq, expected_cert, "") {
  return arg && expected_cert &&
         base::ranges::equal(arg->der_cert().AsSpan(),
                             expected_cert->der_cert().AsSpan());
}

TEST(TrustStoreWin, GetIssuersNoIssuerFound) {
  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ParsedCertificateList issuers;
  scoped_refptr<ParsedCertificate> cert =
      ParseCertFromFile("multi-root-A-by-B.pem");
  ASSERT_TRUE(cert);
  trust_store_win->SyncGetIssuersOf(cert.get(), &issuers);
  ASSERT_EQ(0U, issuers.size());
}

TEST(TrustStoreWin, GetIssuersSingleIssuerFoundFromIntermediates) {
  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ParsedCertificateList issuers;
  scoped_refptr<ParsedCertificate> cert =
      ParseCertFromFile("multi-root-B-by-F.pem");
  ASSERT_TRUE(cert);
  trust_store_win->SyncGetIssuersOf(cert.get(), &issuers);
  ASSERT_EQ(1U, issuers.size());
  EXPECT_THAT(issuers, testing::UnorderedElementsAre(
                           ParsedCertEq(ParseCertFromFile(kMultiRootFByE))));
}

TEST(TrustStoreWin, GetIssuersSingleIssuerFoundFromRoot) {
  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ParsedCertificateList issuers;
  scoped_refptr<ParsedCertificate> cert = ParseCertFromFile(kMultiRootDByD);
  ASSERT_TRUE(cert);
  trust_store_win->SyncGetIssuersOf(cert.get(), &issuers);
  ASSERT_EQ(1U, issuers.size());
  EXPECT_THAT(issuers, testing::UnorderedElementsAre(ParsedCertEq(cert)));
}

TEST(TrustStoreWin, GetIssuersMultipleIssuersFound) {
  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ParsedCertificateList issuers;
  scoped_refptr<ParsedCertificate> cert =
      ParseCertFromFile("multi-root-B-by-C.pem");
  ASSERT_TRUE(cert);
  trust_store_win->SyncGetIssuersOf(cert.get(), &issuers);
  ASSERT_EQ(2U, issuers.size());
  EXPECT_THAT(issuers, testing::UnorderedElementsAre(
                           ParsedCertEq(ParseCertFromFile(kMultiRootCByD)),
                           ParsedCertEq(ParseCertFromFile(kMultiRootCByE))));
}

}  // namespace
}  // namespace net
