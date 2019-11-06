// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_TEST_CERTIFICATE_PROVIDER_EXTENSION_H_
#define CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_TEST_CERTIFICATE_PROVIDER_EXTENSION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace base {
class Value;
}

namespace content {
class BrowserContext;
}

namespace net {
class X509Certificate;
}

// This class provides the C++ side of the test certificate provider extension's
// implementation (the JavaScript side is in
// chrome/test/data/extensions/test_certificate_provider).
//
// It subscribes itself for requests from the JavaScript side of the extension,
// and implements the cryptographic operations using the "client_1" test
// certificate and private key (see src/net/data/ssl/certificates). The
// signature algorithm is currently hardcoded to SHA-256.
class TestCertificateProviderExtension final
    : public content::NotificationObserver {
 public:
  TestCertificateProviderExtension(content::BrowserContext* browser_context,
                                   const std::string& extension_id);
  ~TestCertificateProviderExtension() override;

  const scoped_refptr<net::X509Certificate>& certificate() const {
    return certificate_;
  }

 private:
  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  base::Value HandleCertificatesRequest();
  base::Value HandleSignDigestRequest(const base::Value& sign_request);

  content::BrowserContext* const browser_context_;
  const std::string extension_id_;
  scoped_refptr<net::X509Certificate> certificate_;
  bssl::UniquePtr<EVP_PKEY> private_key_;
  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(TestCertificateProviderExtension);
};

#endif  // CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_TEST_CERTIFICATE_PROVIDER_EXTENSION_H_
