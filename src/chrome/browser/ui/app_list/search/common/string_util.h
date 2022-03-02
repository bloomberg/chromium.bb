// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_STRING_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_STRING_UTIL_H_

#include <string>

namespace app_list {

// Normalizes training targets by removing any scheme prefix and trailing slash:
// "arc://[id]/" to "[id]". This is necessary because apps launched from
// different parts of the launcher have differently formatted IDs.
std::string NormalizeId(const std::string& id);

// Remove the Arc app shortcut label from an app ID, if it exists, so that
// "[app]/[label]" becomes "[app]".
std::string RemoveAppShortcutLabel(const std::string& id);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_STRING_UTIL_H_
