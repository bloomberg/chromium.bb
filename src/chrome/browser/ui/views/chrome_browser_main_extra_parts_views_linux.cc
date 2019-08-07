// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_linux.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/linux_ui/linux_ui_factory.h"
#include "chrome/browser/ui/views/theme_profile_key.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/native_theme/native_theme_dark_aura.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/native_widget_aura.h"

namespace {

ui::NativeTheme* GetNativeThemeForWindow(aura::Window* window) {
  if (!window)
    return nullptr;

  Profile* profile = GetThemeProfileForWindow(window);

  // If using the system (GTK) theme, don't use an Aura NativeTheme at all.
  // NB: ThemeService::UsingSystemTheme() might lag behind this pref. See
  // http://crbug.com/585522
  if (!profile || (!profile->IsSupervised() &&
                   profile->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme))) {
    return nullptr;
  }

  // Use a dark theme for incognito browser windows that aren't
  // custom-themed. Otherwise, normal Aura theme.
  if (profile->IsIncognitoProfile() &&
      ThemeServiceFactory::GetForProfile(profile)->UsingDefaultTheme() &&
      BrowserView::GetBrowserViewForNativeWindow(window)) {
    return ui::NativeThemeDarkAura::instance();
  }

  return ui::NativeTheme::GetInstanceForNativeUi();
}

}  // namespace

ChromeBrowserMainExtraPartsViewsLinux::ChromeBrowserMainExtraPartsViewsLinux() =
    default;

ChromeBrowserMainExtraPartsViewsLinux::
    ~ChromeBrowserMainExtraPartsViewsLinux() = default;

void ChromeBrowserMainExtraPartsViewsLinux::PreEarlyInitialization() {
  views::LinuxUI* linux_ui = views::BuildLinuxUI();
  if (!linux_ui)
    return;

  linux_ui->SetNativeThemeOverride(
      base::BindRepeating(&GetNativeThemeForWindow));
  views::LinuxUI::SetInstance(linux_ui);
}

void ChromeBrowserMainExtraPartsViewsLinux::ToolkitInitialized() {
  ChromeBrowserMainExtraPartsViews::ToolkitInitialized();
  auto* instance = views::LinuxUI::instance();
  if (instance)
    instance->Initialize();
}

void ChromeBrowserMainExtraPartsViewsLinux::PreCreateThreads() {
  // Update the device scale factor before initializing views
  // because its display::Screen instance depends on it.
  auto* instance = views::LinuxUI::instance();
  if (instance)
    instance->UpdateDeviceScaleFactor();
  ChromeBrowserMainExtraPartsViews::PreCreateThreads();
}
