// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_EMAIL_PROVIDERS_LIST_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_EMAIL_PROVIDERS_LIST_H_

#include <vector>

#include "chrome/browser/ui/webui/welcome/nux/bookmark_item.h"

namespace nux {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum EmailProviders {
  kGmail = 0,
  kYahoo = 1,
  kOutlook = 2,
  kAol = 3,
  kiCloud = 4,
  kCount,
};

std::vector<BookmarkItem> GetCurrentCountryEmailProviders();

}  // namespace nux

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_EMAIL_PROVIDERS_LIST_H_
