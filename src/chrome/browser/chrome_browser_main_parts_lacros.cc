// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_parts_lacros.h"

#include "base/check.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/metrics_reporting_observer.h"
#include "chrome/browser/lacros/prefs_ash_observer.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/lacros/dbus/lacros_dbus_helper.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/common/result_codes.h"
#include "ui/wm/core/wm_core_switches.h"

ChromeBrowserMainPartsLacros::ChromeBrowserMainPartsLacros(
    content::MainFunctionParams parameters,
    StartupData* startup_data)
    : ChromeBrowserMainPartsLinux(std::move(parameters), startup_data) {}

ChromeBrowserMainPartsLacros::~ChromeBrowserMainPartsLacros() = default;

int ChromeBrowserMainPartsLacros::PreEarlyInitialization() {
  int result = ChromeBrowserMainPartsLinux::PreEarlyInitialization();
  if (result != content::RESULT_CODE_NORMAL_EXIT)
    return result;

  // The observer sets the initial metrics consent state, then observes ash
  // for updates. Create it here because local state is required to check for
  // policy overrides.
  DCHECK(g_browser_process->local_state());
  metrics_reporting_observer_ = std::make_unique<MetricsReportingObserver>(
      g_browser_process->local_state());
  metrics_reporting_observer_->Init();
  prefs_ash_observer_ =
      std::make_unique<PrefsAshObserver>(g_browser_process->local_state());
  prefs_ash_observer_->Init();

  return content::RESULT_CODE_NORMAL_EXIT;
}

void ChromeBrowserMainPartsLacros::PreProfileInit() {
  ChromeBrowserMainPartsLinux::PreProfileInit();

  // Apply specific flags if this is a Web Kiosk session.
  if (chromeos::LacrosService::Get()->init_params()->session_type ==
      crosapi::mojom::SessionType::kWebKioskSession) {
    // Hide certain system UI elements.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kForceAppMode);

    // Disable window animation since kiosk app runs in a single full screen
    // window and window animation causes start-up janks.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        wm::switches::kWindowAnimationsDisabled);
  }

  // Initialize TtsPlatform so that TtsPlatformImplLacros can observe the
  // ProfileManager for OnProfileAdded event before the profile is loaded.
  content::TtsPlatform::GetInstance();
}

void ChromeBrowserMainPartsLacros::PostDestroyThreads() {
  chromeos::LacrosShutdownDBus();

  ChromeBrowserMainPartsLinux::PostDestroyThreads();
}
