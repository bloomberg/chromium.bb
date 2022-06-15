// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/system_trust_store.h"

#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/net_buildflags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/cert/internal/trust_store_chrome.h"
#endif  // CHROME_ROOT_STORE_SUPPORTED

namespace net {

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/data/ssl/chrome_root_store/chrome-root-store-test-data-inc.cc"

TEST(SystemTrustStoreChrome, SystemDistrustOverridesChromeTrust) {
  CertificateList certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_store.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_GE(certs.size(), 1u);

  scoped_refptr<ParsedCertificate> root = ParsedCertificate::Create(
      bssl::UpRef(certs[0]->cert_buffer()),
      x509_util::DefaultParseCertificateOptions(), nullptr);
  ASSERT_TRUE(root);

  auto test_system_trust_store = std::make_unique<TrustStoreInMemory>();
  auto* test_system_trust_store_ptr = test_system_trust_store.get();

  std::unique_ptr<TrustStoreChrome> test_trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList),
          /*version=*/1);

  std::unique_ptr<SystemTrustStore> system_trust_store_chrome =
      CreateSystemTrustStoreChromeForTesting(
          std::move(test_trust_store_chrome),
          std::move(test_system_trust_store));

  // With no trust settings in the fake system trust store, the cert is trusted
  // by the test chrome root store.
  EXPECT_TRUE(system_trust_store_chrome->GetTrustStore()
                  ->GetTrust(root.get(), /*debug_data=*/nullptr)
                  .IsTrustAnchor());

  // Adding a distrust entry in the fake system trust store should override the
  // trust in the chrome root store.
  test_system_trust_store_ptr->AddDistrustedCertificateForTest(root);
  EXPECT_TRUE(system_trust_store_chrome->GetTrustStore()
                  ->GetTrust(root.get(), /*debug_data=*/nullptr)
                  .IsDistrusted());
}
#endif  // CHROME_ROOT_STORE_SUPPORTED
        //
}  // namespace net
