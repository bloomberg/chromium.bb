// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/client_hints.h"
#include "content/browser/client_hints/client_hints.h"

namespace content {

void AddClientHintsHeadersToPrefetchNavigation(
    const GURL& url,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    bool is_ua_override_on,
    bool is_javascript_enabled) {
  AddPrefetchNavigationRequestClientHintsHeaders(url, headers, context,
                                                 delegate, is_ua_override_on,
                                                 is_javascript_enabled);
}

}  // namespace content
