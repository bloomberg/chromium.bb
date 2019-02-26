// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/prefs/pref_watcher.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/common/renderer_preferences.h"

namespace {

// The list of prefs we want to observe.
const char* const kWebPrefsToObserve[] = {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    prefs::kAnimationPolicy,
#endif
    prefs::kDataSaverEnabled,
    prefs::kDefaultCharset,
    prefs::kDisable3DAPIs,
    prefs::kEnableHyperlinkAuditing,
    prefs::kWebKitAllowRunningInsecureContent,
    prefs::kWebKitDefaultFixedFontSize,
    prefs::kWebKitDefaultFontSize,
    prefs::kWebKitDomPasteEnabled,
#if defined(OS_ANDROID)
    prefs::kWebKitFontScaleFactor,
    prefs::kWebKitForceEnableZoom,
    prefs::kWebKitPasswordEchoEnabled,
#endif
    prefs::kWebKitJavascriptEnabled,
    prefs::kWebKitLoadsImagesAutomatically,
    prefs::kWebKitMinimumFontSize,
    prefs::kWebKitMinimumLogicalFontSize,
    prefs::kWebKitPluginsEnabled,
    prefs::kWebkitTabsToLinks,
    prefs::kWebKitTextAreasAreResizable,
    prefs::kWebKitWebSecurityEnabled,
};

const int kWebPrefsToObserveLength = base::size(kWebPrefsToObserve);

}  // namespace

// Watching all these settings per tab is slow when a user has a lot of tabs and
// and they use session restore. So watch them once per profile.
// http://crbug.com/452693
PrefWatcher::PrefWatcher(Profile* profile) : profile_(profile) {
  pref_change_registrar_.Init(profile_->GetPrefs());

  base::RepeatingClosure renderer_callback = base::BindRepeating(
      &PrefWatcher::UpdateRendererPreferences, base::Unretained(this));
  pref_change_registrar_.Add(prefs::kAcceptLanguages, renderer_callback);
  pref_change_registrar_.Add(prefs::kEnableDoNotTrack, renderer_callback);
  pref_change_registrar_.Add(prefs::kEnableReferrers, renderer_callback);
  pref_change_registrar_.Add(prefs::kEnableEncryptedMedia, renderer_callback);
  pref_change_registrar_.Add(prefs::kWebRTCMultipleRoutesEnabled,
                             renderer_callback);
  pref_change_registrar_.Add(prefs::kWebRTCNonProxiedUdpEnabled,
                             renderer_callback);
  pref_change_registrar_.Add(prefs::kWebRTCIPHandlingPolicy, renderer_callback);
  pref_change_registrar_.Add(prefs::kWebRTCUDPPortRange, renderer_callback);

#if !defined(OS_MACOSX)
  pref_change_registrar_.Add(prefs::kFullscreenAllowed, renderer_callback);
#endif

  PrefChangeRegistrar::NamedChangeCallback webkit_callback =
      base::BindRepeating(&PrefWatcher::OnWebPrefChanged,
                          base::Unretained(this));
  for (int i = 0; i < kWebPrefsToObserveLength; ++i) {
    const char* pref_name = kWebPrefsToObserve[i];
    pref_change_registrar_.Add(pref_name, webkit_callback);
  }
}

PrefWatcher::~PrefWatcher() = default;

void PrefWatcher::RegisterHelper(PrefsTabHelper* helper) {
  tab_helpers_.insert(helper);
}

void PrefWatcher::UnregisterHelper(PrefsTabHelper* helper) {
  tab_helpers_.erase(helper);
}

void PrefWatcher::RegisterWatcherForWorkers(
    content::mojom::RendererPreferenceWatcherPtr worker_watcher) {
  worker_watchers_.AddPtr(std::move(worker_watcher));
}

void PrefWatcher::Shutdown() {
  pref_change_registrar_.RemoveAll();
}

void PrefWatcher::UpdateRendererPreferences() {
  for (auto* helper : tab_helpers_)
    helper->UpdateRendererPreferences();

  content::RendererPreferences prefs;
  renderer_preferences_util::UpdateFromSystemSettings(&prefs, profile_);
  worker_watchers_.ForAllPtrs(
      [&prefs](content::mojom::RendererPreferenceWatcher* watcher) {
        watcher->NotifyUpdate(prefs);
      });
}

void PrefWatcher::OnWebPrefChanged(const std::string& pref_name) {
  for (auto* helper : tab_helpers_)
    helper->OnWebPrefChanged(pref_name);
}

// static
PrefWatcher* PrefWatcherFactory::GetForProfile(Profile* profile) {
  return static_cast<PrefWatcher*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PrefWatcherFactory* PrefWatcherFactory::GetInstance() {
  return base::Singleton<PrefWatcherFactory>::get();
}

PrefWatcherFactory::PrefWatcherFactory()
    : BrowserContextKeyedServiceFactory(
          "PrefWatcher",
          BrowserContextDependencyManager::GetInstance()) {}

PrefWatcherFactory::~PrefWatcherFactory() = default;

KeyedService* PrefWatcherFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new PrefWatcher(Profile::FromBrowserContext(browser_context));
}

content::BrowserContext* PrefWatcherFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

// static
PrefWatcher* PrefWatcher::Get(Profile* profile) {
  return PrefWatcherFactory::GetForProfile(profile);
}
