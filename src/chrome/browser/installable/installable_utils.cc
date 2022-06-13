// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_utils.h"

#include "build/build_config.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/shortcut_helper.h"
#else
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#endif

bool DoesOriginContainAnyInstalledWebApp(
    content::BrowserContext* browser_context,
    const GURL& origin) {
  DCHECK_EQ(origin, origin.DeprecatedGetOriginAsURL());
#if defined(OS_ANDROID)
  return ShortcutHelper::DoesOriginContainAnyInstalledWebApk(origin);
#else
  auto* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(browser_context));
  // TODO: Change this method to async, or document that the caller must know
  // that WebAppProvider is started.
  if (!provider || !provider->on_registry_ready().is_signaled())
    return false;
  return provider->registrar().DoesScopeContainAnyApp(origin);
#endif
}

std::set<GURL> GetOriginsWithInstalledWebApps(
    content::BrowserContext* browser_context) {
#if defined(OS_ANDROID)
  return ShortcutHelper::GetOriginsWithInstalledWebApksOrTwas();
#else
  auto* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(browser_context));
  // TODO: Change this method to async, or document that the caller must know
  // that WebAppProvider is started.
  if (!provider || !provider->on_registry_ready().is_signaled())
    return std::set<GURL>();
  const web_app::WebAppRegistrar& registrar = provider->registrar();
  auto app_ids = registrar.GetAppIds();
  std::set<GURL> installed_origins;
  for (auto& app_id : app_ids) {
    GURL origin = registrar.GetAppScope(app_id).DeprecatedGetOriginAsURL();
    DCHECK(origin.is_valid());
    if (origin.SchemeIs(url::kHttpScheme))
      installed_origins.emplace(origin);
  }
  return installed_origins;
#endif
}
