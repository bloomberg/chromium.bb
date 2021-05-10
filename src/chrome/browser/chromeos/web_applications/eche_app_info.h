// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_ECHE_APP_INFO_H_
#define CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_ECHE_APP_INFO_H_

#include <memory>

struct WebApplicationInfo;

// Return a WebApplicationInfo used to install the app.
std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForEcheApp();

#endif  // CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_ECHE_APP_INFO_H_
