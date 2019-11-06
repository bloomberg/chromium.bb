// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CHROME_MOCK_CERT_VERIFIER_H_
#define CHROME_BROWSER_SSL_CHROME_MOCK_CERT_VERIFIER_H_

#include "content/public/test/content_mock_cert_verifier.h"

// Helper class for use by tests that already derive from
// InProcessBrowserTest so can't use CertVerifierBrowserTest.
class ChromeMockCertVerifier : public content::ContentMockCertVerifier {
 public:
  ChromeMockCertVerifier();
  ~ChromeMockCertVerifier() override;
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeMockCertVerifier);
};

#endif  // CHROME_BROWSER_SSL_CHROME_MOCK_CERT_VERIFIER_H_
