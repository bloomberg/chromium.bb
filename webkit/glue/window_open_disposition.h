// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WINDOW_OPEN_DISPOSITION_H_
#define WEBKIT_GLUE_WINDOW_OPEN_DISPOSITION_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/WebNavigationPolicy.h"
#include "webkit/glue/webkit_glue_export.h"

enum WindowOpenDisposition {
  UNKNOWN,
  SUPPRESS_OPEN,
  CURRENT_TAB,
  // Indicates that only one tab with the url should exist in the same window.
  SINGLETON_TAB,
  NEW_FOREGROUND_TAB,
  NEW_BACKGROUND_TAB,
  NEW_POPUP,
  NEW_WINDOW,
  SAVE_TO_DISK,
  OFF_THE_RECORD,
  IGNORE_ACTION
};

// Conversion function:
WEBKIT_GLUE_EXPORT WindowOpenDisposition NavigationPolicyToDisposition(
    WebKit::WebNavigationPolicy policy);

#endif  // WEBKIT_GLUE_WINDOW_OPEN_DISPOSITION_H_
