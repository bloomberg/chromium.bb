// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CLEAR_SITE_DATA_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_CLEAR_SITE_DATA_UTILS_H_

#include "base/callback_forward.h"
#include "content/common/content_export.h"

namespace url {
class Origin;
}

namespace content {
class BrowserContext;

// Removes browsing data associated with |origin|. Used when the Clear-Site-Data
// header is sent.
// Has to be called on the UI thread and will execute |callback| on the UI
// thread when done.
CONTENT_EXPORT void ClearSiteData(
    const base::RepeatingCallback<BrowserContext*()>& browser_context_getter,
    const url::Origin& origin,
    bool clear_cookies,
    bool clear_storage,
    bool clear_cache,
    bool avoid_closing_connections,
    base::OnceClosure callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CLEAR_SITE_DATA_UTILS_H_
