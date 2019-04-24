// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux/email_providers_list.h"

#include "chrome/browser/ui/webui/welcome/nux/bookmark_item.h"
#include "chrome/grit/onboarding_welcome_resources.h"
#include "components/country_codes/country_codes.h"

namespace nux {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EmailProviders {
  kGmail = 0,
  kYahoo = 1,
  kOutlook = 2,
  kAol = 3,
  kiCloud = 4,
  kCount,
};

std::vector<BookmarkItem> GetCurrentCountryEmailProviders() {
  switch (country_codes::GetCurrentCountryID()) {
    case country_codes::CountryCharsToCountryID('U', 'S'): {
      return {
          {static_cast<int>(EmailProviders::kGmail), "Gmail", "gmail",
           "https://accounts.google.com/b/0/AddMailService",
           IDR_NUX_EMAIL_GMAIL_1X},
          {static_cast<int>(EmailProviders::kYahoo), "Yahoo", "yahoo",
           "https://mail.yahoo.com", IDR_NUX_EMAIL_YAHOO_1X},
          {static_cast<int>(EmailProviders::kOutlook), "Outlook", "outlook",
           "https://login.live.com/login.srf?", IDR_NUX_EMAIL_OUTLOOK_1X},
          {static_cast<int>(EmailProviders::kAol), "AOL", "aol",
           "https://mail.aol.com", IDR_NUX_EMAIL_AOL_1X},
          {static_cast<int>(EmailProviders::kiCloud), "iCloud", "icloud",
           "https://www.icloud.com/mail", IDR_NUX_EMAIL_ICLOUD_1X},
      };
    }

      // TODO(scottchen): define all supported countries here.

    default: {
      // TODO(scottchen): examine if we want these US providers as default.
      return {
          {static_cast<int>(EmailProviders::kGmail), "Gmail", "gmail",
           "https://accounts.google.com/b/0/AddMailService",
           IDR_NUX_EMAIL_GMAIL_1X},
          {static_cast<int>(EmailProviders::kYahoo), "Yahoo", "yahoo",
           "https://mail.yahoo.com", IDR_NUX_EMAIL_YAHOO_1X},
          {static_cast<int>(EmailProviders::kOutlook), "Outlook", "outlook",
           "https://login.live.com/login.srf?", IDR_NUX_EMAIL_OUTLOOK_1X},
          {static_cast<int>(EmailProviders::kAol), "AOL", "aol",
           "https://mail.aol.com", IDR_NUX_EMAIL_AOL_1X},
          {static_cast<int>(EmailProviders::kiCloud), "iCloud", "icloud",
           "https://www.icloud.com/mail", IDR_NUX_EMAIL_ICLOUD_1X},
      };
    }
  }
}

int GetNumberOfEmailProviders() {
  return static_cast<int>(EmailProviders::kCount);
}

}  // namespace nux
