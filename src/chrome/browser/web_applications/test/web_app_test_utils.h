// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_UTILS_H_

#include <memory>

#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"

struct WebApplicationInfo;
class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {
namespace test {

std::unique_ptr<WebApp> CreateWebApp(
    const GURL& start_url = GURL("https://example.com/path"),
    Source::Type source_type = Source::kSync);

std::unique_ptr<WebApp> CreateRandomWebApp(const GURL& base_url,
                                           const uint32_t seed);

void TestAcceptDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    WebAppInstallationAcceptanceCallback acceptance_callback);

void TestDeclineDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    WebAppInstallationAcceptanceCallback acceptance_callback);

}  // namespace test
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_UTILS_H_
