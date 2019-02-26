// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/ntp_util.h"

#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool IsURLNewTabPage(const GURL& url) {
  return url.GetOrigin() == kChromeUINewTabURL;
}

bool IsVisibleURLNewTabPage(web::WebState* web_state) {
  if (!web_state)
    return false;
  return IsURLNewTabPage(web_state->GetVisibleURL());
}
