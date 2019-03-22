// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CHROME_LOAD_PARAMS_H_
#define IOS_CHROME_BROWSER_UI_CHROME_LOAD_PARAMS_H_

#import "ios/web/public/navigation_manager.h"
#include "ui/base/window_open_disposition.h"

// Chrome wrapper around web::NavigationManager::WebLoadParams to add the
// parameters needed by chrome. This is used when a URL is opened.
struct ChromeLoadParams {
 public:
  // The wrapped params.
  web::NavigationManager::WebLoadParams web_params;

  // The disposition of the URL being opened.
  WindowOpenDisposition disposition;

  explicit ChromeLoadParams(
      const web::NavigationManager::WebLoadParams& web_params);
  explicit ChromeLoadParams(const GURL& url);
  ~ChromeLoadParams();

  // Allow copying ChromeLoadParams.
  ChromeLoadParams(const ChromeLoadParams& other);
  ChromeLoadParams& operator=(const ChromeLoadParams& other);
};

#endif  // IOS_CHROME_BROWSER_UI_CHROME_LOAD_PARAMS_H_
