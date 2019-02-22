// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service.h"

#include "chrome/browser/browser_switcher/alternative_browser_launcher.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace browser_switcher {

BrowserSwitcherService::BrowserSwitcherService(PrefService* prefs)
    : launcher_(nullptr), sitelist_(nullptr), prefs_(prefs) {
  DCHECK(prefs_);
}

BrowserSwitcherService::~BrowserSwitcherService() {}

AlternativeBrowserLauncher* BrowserSwitcherService::launcher() {
  if (!launcher_)
    launcher_ = std::make_unique<AlternativeBrowserLauncherImpl>(prefs_);
  return launcher_.get();
}

BrowserSwitcherSitelist* BrowserSwitcherService::sitelist() {
  if (!sitelist_)
    sitelist_ = std::make_unique<BrowserSwitcherSitelistImpl>(prefs_);
  return sitelist_.get();
}

void BrowserSwitcherService::SetLauncherForTesting(
    std::unique_ptr<AlternativeBrowserLauncher> launcher) {
  launcher_ = std::move(launcher);
}

void BrowserSwitcherService::SetSitelistForTesting(
    std::unique_ptr<BrowserSwitcherSitelist> sitelist) {
  sitelist_ = std::move(sitelist);
}

}  // namespace browser_switcher
