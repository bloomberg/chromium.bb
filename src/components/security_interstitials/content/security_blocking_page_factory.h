// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_BLOCKING_PAGE_FACTORY_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_BLOCKING_PAGE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/security_interstitials/content/bad_clock_blocking_page.h"
#include "components/security_interstitials/content/blocked_interception_blocking_page.h"
#include "components/security_interstitials/content/captive_portal_blocking_page.h"
#include "components/security_interstitials/content/legacy_tls_blocking_page.h"
#include "components/security_interstitials/content/mitm_software_blocking_page.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/ssl_blocking_page_base.h"

// An interface that the embedder implements to supply instances of security
// blocking pages that are configured for that embedder.
class SecurityBlockingPageFactory {
 public:
  SecurityBlockingPageFactory() = default;
  virtual ~SecurityBlockingPageFactory() = default;

  // Creates an SSL blocking page. |options_mask| must be a bitwise mask of
  // SSLErrorUI::SSLErrorOptionsMask values.
  virtual std::unique_ptr<SSLBlockingPage> CreateSSLPage(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      int options_mask,
      const base::Time& time_triggered,
      const GURL& support_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter) = 0;

  // Creates a captive portal blocking page.
  virtual std::unique_ptr<CaptivePortalBlockingPage>
  CreateCaptivePortalBlockingPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      const GURL& login_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const net::SSLInfo& ssl_info,
      int cert_error) = 0;

  // Creates a bad clock blocking page.
  virtual std::unique_ptr<BadClockBlockingPage> CreateBadClockBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      const base::Time& time_triggered,
      ssl_errors::ClockState clock_state,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter) = 0;

  // Creates a legacy TLS blocking page.
  virtual std::unique_ptr<LegacyTLSBlockingPage> CreateLegacyTLSBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const GURL& request_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const net::SSLInfo& ssl_info) = 0;

  // Creates a man-in-the-middle software blocking page.
  virtual std::unique_ptr<MITMSoftwareBlockingPage>
  CreateMITMSoftwareBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const GURL& request_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const net::SSLInfo& ssl_info,
      const std::string& mitm_software_name) = 0;

  // Creates a blocked interception blocking page.
  virtual std::unique_ptr<BlockedInterceptionBlockingPage>
  CreateBlockedInterceptionBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const GURL& request_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const net::SSLInfo& ssl_info) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityBlockingPageFactory);
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_BLOCKING_PAGE_FACTORY_H_
