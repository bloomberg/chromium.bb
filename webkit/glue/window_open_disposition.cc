// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/window_open_disposition.h"

#include "base/logging.h"

// The macro dance here allows us to only express the mapping once.
#define MAPPINGS(MAP) \
  MAP(WebNavigationPolicyIgnore, IGNORE_ACTION) \
  MAP(WebNavigationPolicyDownload, SAVE_TO_DISK) \
  MAP(WebNavigationPolicyCurrentTab, CURRENT_TAB) \
  MAP(WebNavigationPolicyNewBackgroundTab, NEW_BACKGROUND_TAB) \
  MAP(WebNavigationPolicyNewForegroundTab, NEW_FOREGROUND_TAB) \
  MAP(WebNavigationPolicyNewWindow, NEW_WINDOW) \
  MAP(WebNavigationPolicyNewPopup, NEW_POPUP)

#define POLICY_TO_DISPOSITION(policy, disposition) \
  case WebKit::policy: return disposition;

WindowOpenDisposition NavigationPolicyToDisposition(
    WebKit::WebNavigationPolicy policy) {
  switch (policy) {
    MAPPINGS(POLICY_TO_DISPOSITION)
  default:
    NOTREACHED() << "Unexpected WebNavigationPolicy";
    return IGNORE_ACTION;
  }
}

#define DISPOSITION_TO_POLICY(policy, disposition) \
  case disposition: return WebKit::policy;

WebKit::WebNavigationPolicy DispositionToNavigationPolicy(
    WindowOpenDisposition disposition) {
  switch (disposition) {
    MAPPINGS(DISPOSITION_TO_POLICY)
  default:
    NOTREACHED() << "Unexpected WindowOpenDisposition";
    return WebKit::WebNavigationPolicyIgnore;
  }
}
