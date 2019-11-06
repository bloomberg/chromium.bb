// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOG_SOURCE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOG_SOURCE_H_

#include "url/gurl.h"

// Object used to capture the source of a JavaScript dialog.
class JavaScriptDialogSource {
 public:
  JavaScriptDialogSource(const GURL& url, bool is_main_frame);
  ~JavaScriptDialogSource();

  // The URL of the page requesting the JavaScript dialog.
  const GURL& url() const { return url_; }
  // Whether or not the requesting page is in the main frame.
  bool is_main_frame() const { return is_main_frame_; }

 private:
  GURL url_;
  bool is_main_frame_ = false;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOG_SOURCE_H_
