// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/ssl_validity_checker.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/core/url_util.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

namespace payments {

// static
bool SslValidityChecker::IsSslCertificateValid(
    content::WebContents* web_contents) {
  if (!web_contents)
    return false;

  SecurityStateTabHelper::CreateForWebContents(web_contents);
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  DCHECK(helper);
  security_state::SecurityLevel security_level = helper->GetSecurityLevel();
  return security_level == security_state::EV_SECURE ||
         security_level == security_state::SECURE ||
         security_level == security_state::SECURE_WITH_POLICY_INSTALLED_CERT ||
         // No early return, so the other code is exercised in tests, too.
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kIgnoreCertificateErrors);
}

// static
bool SslValidityChecker::IsValidPageInPaymentHandlerWindow(
    content::WebContents* web_contents) {
  if (!web_contents)
    return false;

  GURL url = web_contents->GetLastCommittedURL();
  if (!UrlUtil::IsValidUrlInPaymentHandlerWindow(url))
    return false;

  if (url.SchemeIsCryptographic())
    return IsSslCertificateValid(web_contents);

  return true;
}

}  // namespace payments
