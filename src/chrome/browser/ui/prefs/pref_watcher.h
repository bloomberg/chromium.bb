// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PREFS_PREF_WATCHER_H_
#define CHROME_BROWSER_UI_PREFS_PREF_WATCHER_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/common/renderer_preference_watcher.mojom.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

class Profile;
class PrefsTabHelper;

namespace content {
class RendererPreferenceWatcher;
}  // namespace content

// Watches updates in WebKitPreferences and RendererPreferences, and notifies
// tab helpers and watchers for workers of those updates.
class PrefWatcher : public KeyedService {
 public:
  explicit PrefWatcher(Profile* profile);
  ~PrefWatcher() override;

  static PrefWatcher* Get(Profile* profile);

  void RegisterHelper(PrefsTabHelper* helper);
  void UnregisterHelper(PrefsTabHelper* helper);
  void RegisterWatcherForWorkers(
      content::mojom::RendererPreferenceWatcherPtr worker_watcher);

 private:
  // KeyedService overrides:
  void Shutdown() override;

  void UpdateRendererPreferences();
  void OnWebPrefChanged(const std::string& pref_name);

  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;

  // |tab_helpers_| observe changes in WebKitPreferences and
  // RendererPreferences.
  std::set<PrefsTabHelper*> tab_helpers_;

  // |worker_watchers_| observe changes in RendererPreferences.
  mojo::InterfacePtrSet<content::mojom::RendererPreferenceWatcher>
      worker_watchers_;
};

class PrefWatcherFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PrefWatcher* GetForProfile(Profile* profile);
  static PrefWatcherFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PrefWatcherFactory>;

  PrefWatcherFactory();
  ~PrefWatcherFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_PREFS_PREF_WATCHER_H_
