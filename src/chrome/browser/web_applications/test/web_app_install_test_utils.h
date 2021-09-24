// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_TEST_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/common/buildflags.h"
#include "components/webapps/browser/installable/installable_metrics.h"

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
#include "components/services/app_service/public/cpp/url_handler_info.h"
#endif

class GURL;
class Profile;

namespace web_app {

class WebAppProvider;

namespace test {

// Start the WebAppProvider and subsystems, and wait for startup to complete.
// Disables auto-install of system web apps and default web apps. Intended for
// unit tests, not browser tests.
void AwaitStartWebAppProviderAndSubsystems(Profile* profile);

// Wait until the provided WebAppProvider is ready.
void WaitUntilReady(WebAppProvider* provider);

AppId InstallDummyWebApp(Profile* profile,
                         const std::string& app_name,
                         const GURL& app_url);

// Synchronous version of WebAppInstallManager::InstallWebAppFromInfo. May be
// used in unit tests and browser tests.
AppId InstallWebApp(Profile* profile,
                    std::unique_ptr<WebApplicationInfo> web_app_info,
                    bool overwrite_existing_manifest_fields = false,
                    webapps::WebappInstallSource install_source =
                        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
// Install a web app with url_handlers then register it with the
// UrlHandlerManager. This is sufficient for testing URL matching and launch
// at startup.
AppId InstallWebAppWithUrlHandlers(
    Profile* profile,
    const GURL& start_url,
    const std::u16string& app_name,
    const std::vector<apps::UrlHandlerInfo>& url_handlers);
#endif

// Synchronously uninstall a web app. May be used in unit tests and browser
// tests.
void UninstallWebApp(Profile* profile, const AppId& app_id);

}  // namespace test
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_TEST_UTILS_H_
