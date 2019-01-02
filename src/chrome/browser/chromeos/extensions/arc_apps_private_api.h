// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_ARC_APPS_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_ARC_APPS_PRIVATE_API_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class ArcAppsPrivateAPI : public BrowserContextKeyedAPI,
                          public EventRouter::Observer,
                          public ArcAppListPrefs::Observer {
 public:
  static BrowserContextKeyedAPIFactory<ArcAppsPrivateAPI>* GetFactoryInstance();

  explicit ArcAppsPrivateAPI(content::BrowserContext* context);
  ~ArcAppsPrivateAPI() override;

  // BrowserContextKeyedAPI:
  void Shutdown() override;

  // EventRouter::Observer:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  // ArcAppListPrefs::Observer:
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;

 private:
  friend class BrowserContextKeyedAPIFactory<ArcAppsPrivateAPI>;

  static const char* service_name() { return "ArcAppsPrivateAPI"; }

  // BrowserContextKeyedAPI:
  static const bool kServiceIsNULLWhileTesting = true;

  content::BrowserContext* const context_;

  ScopedObserver<ArcAppListPrefs, ArcAppsPrivateAPI> scoped_prefs_observer_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppsPrivateAPI);
};

template <>
struct BrowserContextFactoryDependencies<ArcAppsPrivateAPI> {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<ArcAppsPrivateAPI>* factory) {
    factory->DependsOn(
        ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
    factory->DependsOn(ArcAppListPrefsFactory::GetInstance());
  }
};

class ArcAppsPrivateGetLaunchableAppsFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("arcAppsPrivate.getLaunchableApps",
                             ARCAPPSPRIVATE_GETLAUNCHABLEAPPS)

  ArcAppsPrivateGetLaunchableAppsFunction();

 protected:
  ~ArcAppsPrivateGetLaunchableAppsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppsPrivateGetLaunchableAppsFunction);
};

class ArcAppsPrivateLaunchAppFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("arcAppsPrivate.launchApp",
                             ARCAPPSPRIVATE_LAUNCHAPP)

  ArcAppsPrivateLaunchAppFunction();

 protected:
  ~ArcAppsPrivateLaunchAppFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppsPrivateLaunchAppFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_ARC_APPS_PRIVATE_API_H_