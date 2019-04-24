// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_EMAIL_PROVIDERS_LIST_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_EMAIL_PROVIDERS_LIST_H_

#include <vector>

#include "chrome/browser/ui/webui/welcome/nux/bookmark_item.h"

namespace nux {

std::vector<BookmarkItem> GetCurrentCountryEmailProviders();

// Function to avoid exposing enum only for count.
int GetNumberOfEmailProviders();

}  // namespace nux

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_EMAIL_PROVIDERS_LIST_H_
