// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/browser_context_keyed_service_factories.h"
#include "libcef/common/extensions/extensions_util.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/plugins/plugin_prefs_factory.h"
#include "chrome/browser/profiles/renderer_updater_factory.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "extensions/browser/api/alarms/alarm_manager.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/renderer_startup_helper.h"

namespace cef {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  CookieSettingsFactory::GetInstance();
  media_router::ChromeMediaRouterFactory::GetInstance();
  PluginPrefsFactory::GetInstance();
  PrefsTabHelper::GetServiceInstance();
  RendererUpdaterFactory::GetInstance();
  SpellcheckServiceFactory::GetInstance();
  ThemeServiceFactory::GetInstance();

  if (extensions::ExtensionsEnabled()) {
    extensions::AlarmManager::GetFactoryInstance();
    extensions::RendererStartupHelperFactory::GetInstance();
    extensions::StorageFrontend::GetFactoryInstance();
  }
}

}  // namespace cef
