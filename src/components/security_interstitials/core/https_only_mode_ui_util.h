// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_UI_UTIL_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_UI_UTIL_H_

namespace base {
class Value;
}  // namespace base

class GURL;

// Populates |load_time_data| for interstitial HTML.
void PopulateHttpsOnlyModeStringsForBlockingPage(base::Value* load_time_data,
                                                 const GURL& url);

// Values added to get shared interstitial HTML to play nice.
void PopulateHttpsOnlyModeStringsForSharedHTML(base::Value* load_time_data);

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_UI_UTIL_H_
