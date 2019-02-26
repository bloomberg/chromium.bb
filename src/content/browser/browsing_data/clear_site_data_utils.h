// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_DATA_CLEAR_SITE_DATA_UTILS_H_
#define CONTENT_BROWSER_BROWSING_DATA_CLEAR_SITE_DATA_UTILS_H_

#include "base/callback_forward.h"

namespace url {
class Origin;
}

namespace content {
class BrowserContext;
namespace clear_site_data_utils {

// Removes browsing data associated with |origin| when the Clear-Site-Data
// header is sent.
// Has to be called on the UI thread and will execute |callback| on the UI
// thread when done.
// TODO(dullweber): Consider merging back when network service is shipped.
void ClearSiteData(
    const base::RepeatingCallback<BrowserContext*()>& browser_context_getter,
    const url::Origin& origin,
    bool clear_cookies,
    bool clear_storage,
    bool clear_cache,
    base::OnceClosure callback);

}  // namespace clear_site_data_utils
}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_DATA_CLEAR_SITE_DATA_UTILS_H_
