// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/alternative_browser_launcher.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace browser_switcher {

AlternativeBrowserLauncher::~AlternativeBrowserLauncher() {}

AlternativeBrowserLauncherImpl::AlternativeBrowserLauncherImpl(
    PrefService* prefs)
    : AlternativeBrowserLauncherImpl(
          prefs,
          std::make_unique<AlternativeBrowserDriverImpl>()) {}

AlternativeBrowserLauncherImpl::AlternativeBrowserLauncherImpl(
    PrefService* prefs,
    std::unique_ptr<AlternativeBrowserDriver> driver)
    : prefs_(prefs), driver_(std::move(driver)) {
  DCHECK(prefs_);
  DCHECK(driver_);
  change_registrar_.Init(prefs);
  change_registrar_.Add(
      prefs::kAlternativeBrowserPath,
      base::BindRepeating(
          &AlternativeBrowserLauncherImpl::OnAltBrowserPathChanged,
          base::Unretained(this)));
  change_registrar_.Add(
      prefs::kAlternativeBrowserParameters,
      base::BindRepeating(
          &AlternativeBrowserLauncherImpl::OnAltBrowserParametersChanged,
          base::Unretained(this)));
  // Ensure |alt_browser_| is initialized.
  OnAltBrowserPathChanged();
  OnAltBrowserParametersChanged();
}

AlternativeBrowserLauncherImpl::~AlternativeBrowserLauncherImpl() {}

void AlternativeBrowserLauncherImpl::OnAltBrowserPathChanged() {
  // This pref is sensitive. Only set through policies.
  if (!prefs_->IsManagedPreference(prefs::kAlternativeBrowserPath))
    return;

  // This string could be a variable, e.g. "${ie}". Let the driver decide what
  // to do with it.
  driver_->SetBrowserPath(prefs_->GetString(prefs::kAlternativeBrowserPath));
}

void AlternativeBrowserLauncherImpl::OnAltBrowserParametersChanged() {
  // This pref is sensitive. Only set through policies.
  if (!prefs_->IsManagedPreference(prefs::kAlternativeBrowserParameters))
    return;

  // This string could contain a placeholder, e.g. "${url}". Let the driver
  // decide what to do with it.
  driver_->SetBrowserParameters(
      prefs_->GetList(prefs::kAlternativeBrowserParameters));
}

bool AlternativeBrowserLauncherImpl::Launch(const GURL& url) const {
  SCOPED_UMA_HISTOGRAM_TIMER("BrowserSwitcher.LaunchTime");
  bool success = driver_->TryLaunch(url);
  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.LaunchSuccess", success);
  return success;
}

}  // namespace browser_switcher
