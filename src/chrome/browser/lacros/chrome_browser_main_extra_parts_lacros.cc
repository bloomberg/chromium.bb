// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/chrome_browser_main_extra_parts_lacros.h"

#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#include "chrome/browser/lacros/arc/arc_icon_cache.h"
#include "chrome/browser/lacros/automation_manager_lacros.h"
#include "chrome/browser/lacros/browser_service_lacros.h"
#include "chrome/browser/lacros/download_controller_client_lacros.h"
#include "chrome/browser/lacros/drivefs_cache.h"
#include "chrome/browser/lacros/field_trial_observer.h"
#include "chrome/browser/lacros/force_installed_tracker_lacros.h"
#include "chrome/browser/lacros/lacros_butter_bar.h"
#include "chrome/browser/lacros/lacros_extension_apps_controller.h"
#include "chrome/browser/lacros/lacros_extension_apps_publisher.h"
#include "chrome/browser/lacros/lacros_memory_pressure_evaluator.h"
#include "chrome/browser/lacros/screen_orientation_delegate_lacros.h"
#include "chrome/browser/lacros/standalone_browser_test_controller.h"
#include "chrome/browser/lacros/task_manager_lacros.h"
#include "chrome/browser/lacros/web_page_info_lacros.h"
#include "chrome/browser/metrics/structured/chrome_structured_metrics_recorder.h"
#include "chromeos/lacros/lacros_service.h"

ChromeBrowserMainExtraPartsLacros::ChromeBrowserMainExtraPartsLacros() =
    default;
ChromeBrowserMainExtraPartsLacros::~ChromeBrowserMainExtraPartsLacros() =
    default;

void ChromeBrowserMainExtraPartsLacros::PostBrowserStart() {
  automation_manager_ = std::make_unique<AutomationManagerLacros>();
  browser_service_ = std::make_unique<BrowserServiceLacros>();
  butter_bar_ = std::make_unique<LacrosButterBar>();
  download_controller_client_ =
      std::make_unique<DownloadControllerClientLacros>();
  task_manager_provider_ = std::make_unique<crosapi::TaskManagerLacros>();
  kiosk_session_service_ = std::make_unique<KioskSessionServiceLacros>();
  web_page_info_provider_ =
      std::make_unique<crosapi::WebPageInfoProviderLacros>();
  screen_orientation_delegate_ =
      std::make_unique<ScreenOrientationDelegateLacros>();

  memory_pressure::MultiSourceMemoryPressureMonitor* monitor =
      static_cast<memory_pressure::MultiSourceMemoryPressureMonitor*>(
          base::MemoryPressureMonitor::Get());
  if (monitor) {
    monitor->SetSystemEvaluator(std::make_unique<LacrosMemoryPressureEvaluator>(
        monitor->CreateVoter()));
  }

  if (chromeos::LacrosService::Get()->init_params()->publish_chrome_apps) {
    extension_apps_publisher_ =
        std::make_unique<LacrosExtensionAppsPublisher>();
    extension_apps_publisher_->Initialize();
    extension_apps_controller_ =
        std::make_unique<LacrosExtensionAppsController>();
    extension_apps_controller_->Initialize(
        extension_apps_publisher_->publisher());
  }

#if !BUILDFLAG(IS_CHROMEOS_DEVICE)
  // The test controller is only created in test builds AND when Ash's test
  // controller service is available.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::TestController>()) {
    int remote_version = lacros_service->GetInterfaceVersion(
        crosapi::mojom::TestController::Uuid_);
    if (static_cast<uint32_t>(remote_version) >=
        crosapi::mojom::TestController::
            kRegisterStandaloneBrowserTestControllerMinVersion) {
      auto& ash_test_controller =
          lacros_service->GetRemote<crosapi::mojom::TestController>();
      standalone_browser_test_controller_ =
          std::make_unique<StandaloneBrowserTestController>(
              ash_test_controller);
    }
  }
#endif

  arc_icon_cache_ = std::make_unique<ArcIconCache>();
  arc_icon_cache_->Start();

  // Start Lacros' drive mount point path caching, since it is available in Ash.
  drivefs_cache_ = std::make_unique<DriveFsCache>();
  // After construction finishes, start caching.
  drivefs_cache_->Start();

  field_trial_observer_ = std::make_unique<FieldTrialObserver>();
  field_trial_observer_->Start();

  force_installed_tracker_ = std::make_unique<ForceInstalledTrackerLacros>();
  force_installed_tracker_->Start();

  metrics::structured::ChromeStructuredMetricsRecorder::Get()->Initialize();
}
