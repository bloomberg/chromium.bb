// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/chrome_load_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromeLoadParams::ChromeLoadParams(
    const web::NavigationManager::WebLoadParams& web_params)
    : web_params(web_params), disposition(WindowOpenDisposition::CURRENT_TAB) {}

ChromeLoadParams::ChromeLoadParams(const GURL& url)
    : web_params(url), disposition(WindowOpenDisposition::CURRENT_TAB) {}

ChromeLoadParams::~ChromeLoadParams() {}

ChromeLoadParams::ChromeLoadParams(const ChromeLoadParams& other)
    : web_params(other.web_params), disposition(other.disposition) {}

ChromeLoadParams& ChromeLoadParams::operator=(const ChromeLoadParams& other) {
  web_params = other.web_params;
  disposition = other.disposition;

  return *this;
}
