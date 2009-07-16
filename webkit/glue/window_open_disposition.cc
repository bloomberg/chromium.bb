// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/window_open_disposition.h"

#include "base/logging.h"

WindowOpenDisposition NavigationPolicyToDisposition(
    WebKit::WebNavigationPolicy policy) {
  switch (policy) {
    case WebKit::WebNavigationPolicyIgnore:
      return IGNORE_ACTION;
    case WebKit::WebNavigationPolicyDownload:
      return SAVE_TO_DISK;
    case WebKit::WebNavigationPolicyCurrentTab:
      return CURRENT_TAB;
    case WebKit::WebNavigationPolicyNewBackgroundTab:
      return NEW_BACKGROUND_TAB;
    case WebKit::WebNavigationPolicyNewForegroundTab:
      return NEW_FOREGROUND_TAB;
    case WebKit::WebNavigationPolicyNewWindow:
      return NEW_WINDOW;
    case WebKit::WebNavigationPolicyNewPopup:
      return NEW_POPUP;
  default:
    NOTREACHED() << "Unexpected WebNavigationPolicy";
    return IGNORE_ACTION;
  }
}
