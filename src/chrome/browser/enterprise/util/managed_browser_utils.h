// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_UTIL_MANAGED_BROWSER_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_UTIL_MANAGED_BROWSER_UTILS_H_

// Util functions relating to managed browsers.

class Profile;

namespace chrome {
namespace enterprise_util {

// Determines whether policies have been applied to this browser at the profile
// or machine level.
bool HasBrowserPoliciesApplied(Profile* profile);

}  // namespace enterprise_util
}  // namespace chrome

#endif  // CHROME_BROWSER_ENTERPRISE_UTIL_MANAGED_BROWSER_UTILS_H_
