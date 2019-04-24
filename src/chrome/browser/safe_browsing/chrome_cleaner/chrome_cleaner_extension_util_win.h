// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_EXTENSION_UTIL_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_EXTENSION_UTIL_WIN_H_

#include <set>

#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile.h"

namespace safe_browsing {

// Retrieve extension names for |extension_ids| from |profile|'s extension
// registry and add them to |extension_names|. If a name cannot be found for an
// extension ID, a translated string will be added stating the item is an
// unknown extension ID.
void GetExtensionNamesFromIds(Profile* profile,
                              const std::set<base::string16>& extension_ids,
                              std::set<base::string16>* extension_names);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_EXTENSION_UTIL_WIN_H_
