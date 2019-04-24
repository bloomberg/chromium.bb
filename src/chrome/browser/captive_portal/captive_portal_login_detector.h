// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_LOGIN_DETECTOR_H_
#define CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_LOGIN_DETECTOR_H_

#include "base/macros.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"

class Profile;

// Triggers a captive portal test on navigations that may indicate a captive
// portal has been logged into.  Currently only tracks if a page was opened
// at a captive portal tab's login page, and triggers checks every navigation
// until there's no longer a captive portal, relying on the
// CaptivePortalService's throttling to prevent excessive server load.
//
// TODO(mmenke):  If a page has been broken by a captive portal, and it's
// successfully reloaded, trigger a captive portal check.
class CaptivePortalLoginDetector {
 public:
  explicit CaptivePortalLoginDetector(Profile* profile);

  ~CaptivePortalLoginDetector();

  void OnStoppedLoading();
  void OnCaptivePortalResults(
      captive_portal::CaptivePortalResult previous_result,
      captive_portal::CaptivePortalResult result);

  bool is_login_tab() const { return is_login_tab_; }
  void SetIsLoginTab();

 private:
  Profile* profile_;

  // True if this is a login tab.  Set manually, automatically cleared once
  // login is detected.
  bool is_login_tab_;

  // Page is on its first load since being tagged as a login tab.  Used to
  // prevent a second captive portal check on the first load of the login page.
  bool first_login_tab_load_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalLoginDetector);
};

#endif  // CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_LOGIN_DETECTOR_H_
