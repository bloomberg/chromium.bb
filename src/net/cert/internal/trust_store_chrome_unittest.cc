// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_chrome.h"

#include "base/containers/span.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

#include "net/data/ssl/chrome_root_store/chrome-root-store-test-data-inc.cc"

scoped_refptr<ParsedCertificate> ToParsedCertificate(
    const X509Certificate& cert) {
  CertErrors errors;
  scoped_refptr<ParsedCertificate> parsed = ParsedCertificate::Create(
      bssl::UpRef(cert.cert_buffer()),
      x509_util::DefaultParseCertificateOptions(), &errors);
  EXPECT_TRUE(parsed) << errors.ToDebugString();
  return parsed;
}

TEST(TrustStoreChromeTestNoFixture, ContainsCert) {
  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList));

  // Check every certificate in test_store.certs is included.
  CertificateList certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_store.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_EQ(certs.size(), 2u);

  for (const auto& cert : certs) {
    scoped_refptr<ParsedCertificate> parsed = ToParsedCertificate(*cert);
    ASSERT_TRUE(trust_store_chrome->Contains(parsed.get()));
    CertificateTrust trust =
        trust_store_chrome->GetTrust(parsed.get(), /*debug_data=*/nullptr);
    EXPECT_EQ(CertificateTrustType::TRUSTED_ANCHOR, trust.type);
  }

  // Other certificates should not be included.
  scoped_refptr<X509Certificate> other_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ocsp-test-root.pem");
  ASSERT_TRUE(other_cert);
  scoped_refptr<ParsedCertificate> other_parsed =
      ToParsedCertificate(*other_cert);
  ASSERT_FALSE(trust_store_chrome->Contains(other_parsed.get()));
  CertificateTrust trust = trust_store_chrome->GetTrust(other_parsed.get(),
                                                        /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrustType::UNSPECIFIED, trust.type);
}

}  // namespace
}  // namespace net
