// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_WEBAPP_REGISTRY_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_WEBAPP_REGISTRY_H_

#include "base/callback_forward.h"
#include "base/macros.h"

class GURL;

// WebappRegistry is the C++ counterpart of
// org.chromium.chrome.browser.webapp's WebappRegistry in Java.
// All methods in this class which make JNI calls should be declared virtual and
// mocked out in C++ unit tests. The JNI call cannot be made in this environment
// as the Java side will not be initialised.
class WebappRegistry {
 public:
  WebappRegistry() { }
  virtual ~WebappRegistry() { }

  // Cleans up data stored by web apps on URLs matching |url_filter|.
  virtual void UnregisterWebappsForUrls(
      const base::Callback<bool(const GURL&)>& url_filter);

  // Removes history data (last used time and URLs) stored by web apps with
  // URLs matching |url_filter|, whilst leaving other data intact.
  virtual void ClearWebappHistoryForUrls(
      const base::Callback<bool(const GURL&)>& url_filter);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebappRegistry);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_WEBAPP_REGISTRY_H_
