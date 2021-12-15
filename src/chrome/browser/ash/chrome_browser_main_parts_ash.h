// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROME_BROWSER_MAIN_PARTS_ASH_H_
#define CHROME_BROWSER_ASH_CHROME_BROWSER_MAIN_PARTS_ASH_H_

#include <memory>

#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ash/external_metrics.h"
// TODO(https://crbug.com/1164001): remove and use forward declaration.
#include "chrome/browser/ash/net/bluetooth_pref_state_observer.h"
// TODO(https://crbug.com/1164001): remove and use forward declaration.
#include "chrome/browser/ash/net/network_pref_state_observer.h"
// TODO(https://crbug.com/1164001): remove and use forward declaration.
#include "chrome/browser/ash/net/network_throttling_observer.h"
// TODO(https://crbug.com/1164001): remove and use forward declaration.
#include "chrome/browser/ash/network_change_manager_client.h"
#include "chrome/browser/ash/pcie_peripheral/ash_usb_detector.h"
// TODO(https://crbug.com/1164001): remove and use forward declaration.
#include "chrome/browser/ash/system_token_cert_db_initializer.h"
#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_manager.h"
#include "chrome/browser/chrome_browser_main_linux.h"
#include "chrome/browser/memory/memory_kills_monitor.h"
// TODO(https://crbug.com/1164001): remove and use forward declaration.
#include "chromeos/login/session/session_termination_manager.h"
// TODO(https://crbug.com/1164001): remove and use forward declaration.
#include "chromeos/network/fast_transition_observer.h"

class AssistantBrowserDelegateImpl;
class AssistantStateClient;
class ChromeKeyboardControllerClient;
class ImageDownloaderImpl;

namespace arc {
namespace data_snapshotd {
class ArcDataSnapshotdManager;
}  // namespace data_snapshotd

class ArcServiceLauncher;
}  // namespace arc

namespace chromeos {
namespace default_app_order {
class ExternalLoader;
}
}  // namespace chromeos

namespace crosapi {
class BrowserManager;
class CrosapiManager;
}  // namespace crosapi

namespace crostini {
class CrostiniUnsupportedActionNotifier;
class CrosvmMetrics;
}  // namespace crostini

namespace lock_screen_apps {
class StateController;
}

namespace policy {
class LockToSingleUserManager;
}  // namespace policy

namespace ash {
class AccessibilityEventRewriterDelegateImpl;
class ArcKioskAppManager;
class BulkPrintersCalculatorFactory;
class CrosUsbDetector;
class DebugdNotificationHandler;
class DemoModeResourcesRemover;
class EventRewriterDelegateImpl;
class FirmwareUpdateManager;
class GnubbyNotification;
class IdleActionWarningObserver;
class LoginScreenExtensionsLifetimeManager;
class LoginScreenExtensionsStorageCleaner;
class LowDiskNotification;
class PowerMetricsReporter;
class RendererFreezer;
class ShortcutMappingPrefService;
class ShutdownPolicyForwarder;
class WebKioskAppManager;

namespace device_activity {
class DeviceActivityController;
}

namespace internal {
class DBusServices;
}

namespace platform_keys {
class KeyPermissionsManager;
}

namespace power {
class SmartChargingManager;
namespace auto_screen_brightness {
class Controller;
}
namespace ml {
class AdaptiveScreenBrightnessManager;
}
}  // namespace power

namespace quick_pair {
class QuickPairBrowserDelegateImpl;
}

namespace system {
class BreakpadConsentWatcher;
class DarkResumeController;
}  // namespace system

// ChromeBrowserMainParts implementation for chromeos specific code.
// NOTE: Chromeos UI (Ash) support should be added to
// ChromeBrowserMainExtraPartsAsh instead. This class should not depend on
// src/ash or chrome/browser/ui/ash.
class ChromeBrowserMainPartsAsh : public ChromeBrowserMainPartsLinux {
 public:
  ChromeBrowserMainPartsAsh(const content::MainFunctionParams& parameters,
                            StartupData* startup_data);

  ChromeBrowserMainPartsAsh(const ChromeBrowserMainPartsAsh&) = delete;
  ChromeBrowserMainPartsAsh& operator=(const ChromeBrowserMainPartsAsh&) =
      delete;

  ~ChromeBrowserMainPartsAsh() override;

  // ChromeBrowserMainParts overrides.
  int PreEarlyInitialization() override;
  void PreCreateMainMessageLoop() override;
  void PostCreateMainMessageLoop() override;
  int PreMainMessageLoopRun() override;

  // Stages called from PreMainMessageLoopRun.
  void PreProfileInit() override;
  void PostProfileInit() override;
  void PreBrowserStart() override;
  void PostBrowserStart() override;

  // After initialization is finished.
  void OnFirstIdle() override;

  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

 private:
  std::unique_ptr<chromeos::default_app_order::ExternalLoader>
      app_order_loader_;
  std::unique_ptr<NetworkPrefStateObserver> network_pref_state_observer_;
  std::unique_ptr<BluetoothPrefStateObserver> bluetooth_pref_state_observer_;
  std::unique_ptr<IdleActionWarningObserver> idle_action_warning_observer_;
  std::unique_ptr<RendererFreezer> renderer_freezer_;
  std::unique_ptr<PowerMetricsReporter> power_metrics_reporter_;
  std::unique_ptr<FastTransitionObserver> fast_transition_observer_;
  std::unique_ptr<NetworkThrottlingObserver> network_throttling_observer_;
  std::unique_ptr<NetworkChangeManagerClient> network_change_manager_client_;
  std::unique_ptr<DebugdNotificationHandler> debugd_notification_handler_;

  std::unique_ptr<internal::DBusServices> dbus_services_;

  std::unique_ptr<SystemTokenCertDBInitializer>
      system_token_certdb_initializer_;

  std::unique_ptr<ShutdownPolicyForwarder> shutdown_policy_forwarder_;

  std::unique_ptr<EventRewriterDelegateImpl> event_rewriter_delegate_;

  // Handles event dispatch to the accessibility component extensions.
  std::unique_ptr<AccessibilityEventRewriterDelegateImpl>
      accessibility_event_rewriter_delegate_;

  scoped_refptr<ExternalMetrics> external_metrics_;

  std::unique_ptr<arc::ArcServiceLauncher> arc_service_launcher_;

  std::unique_ptr<ImageDownloaderImpl> image_downloader_;

  std::unique_ptr<AssistantStateClient> assistant_state_client_;

  std::unique_ptr<AssistantBrowserDelegateImpl> assistant_delegate_;

  std::unique_ptr<LowDiskNotification> low_disk_notification_;
  std::unique_ptr<ArcKioskAppManager> arc_kiosk_app_manager_;
  std::unique_ptr<WebKioskAppManager> web_kiosk_app_manager_;

  std::unique_ptr<ash::ShortcutMappingPrefService>
      shortcut_mapping_pref_service_;
  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;

  std::unique_ptr<lock_screen_apps::StateController>
      lock_screen_apps_state_controller_;
  std::unique_ptr<crosapi::CrosapiManager> crosapi_manager_;
  std::unique_ptr<crosapi::BrowserManager> browser_manager_;

  std::unique_ptr<power::SmartChargingManager> smart_charging_manager_;

  std::unique_ptr<power::ml::AdaptiveScreenBrightnessManager>
      adaptive_screen_brightness_manager_;

  std::unique_ptr<power::auto_screen_brightness::Controller>
      auto_screen_brightness_controller_;

  std::unique_ptr<DemoModeResourcesRemover> demo_mode_resources_remover_;
  std::unique_ptr<crostini::CrosvmMetrics> crosvm_metrics_;

  std::unique_ptr<AshUsbDetector> ash_usb_detector_;
  std::unique_ptr<CrosUsbDetector> cros_usb_detector_;

  std::unique_ptr<device_activity::DeviceActivityController>
      device_activity_controller_;

  std::unique_ptr<crostini::CrostiniUnsupportedActionNotifier>
      crostini_unsupported_action_notifier_;

  std::unique_ptr<system::DarkResumeController> dark_resume_controller_;

  std::unique_ptr<BulkPrintersCalculatorFactory>
      bulk_printers_calculator_factory_;

  std::unique_ptr<SessionTerminationManager> session_termination_manager_;

  // Set when PreProfileInit() is called. If PreMainMessageLoopRun() exits
  // early, this will be false during PostMainMessageLoopRun(), etc.
  // Used to prevent shutting down classes that were not initialized.
  bool pre_profile_init_called_ = false;

  std::unique_ptr<policy::LockToSingleUserManager> lock_to_single_user_manager_;
  std::unique_ptr<WilcoDtcSupportdManager> wilco_dtc_supportd_manager_;
  std::unique_ptr<LoginScreenExtensionsLifetimeManager>
      login_screen_extensions_lifetime_manager_;
  std::unique_ptr<LoginScreenExtensionsStorageCleaner>
      login_screen_extensions_storage_cleaner_;

  std::unique_ptr<FirmwareUpdateManager> firmware_update_manager_;

  std::unique_ptr<GnubbyNotification> gnubby_notification_;
  std::unique_ptr<system::BreakpadConsentWatcher> breakpad_consent_watcher_;

  std::unique_ptr<arc::data_snapshotd::ArcDataSnapshotdManager>
      arc_data_snapshotd_manager_;

  std::unique_ptr<platform_keys::KeyPermissionsManager>
      system_token_key_permissions_manager_;

  std::unique_ptr<quick_pair::QuickPairBrowserDelegateImpl>
      quick_pair_delegate_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHROME_BROWSER_MAIN_PARTS_ASH_H_
