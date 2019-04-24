// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_UPDATE_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_UPDATE_SERVICE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/chromeos/system/automatic_reboot_manager_observer.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/update_observer.h"

class Profile;

namespace extensions {
class Extension;
}

namespace chromeos {

namespace system {
class AutomaticRebootManager;
}

// This class enforces automatic restart on app and Chrome updates in app mode.
class KioskAppUpdateService : public KeyedService,
                              public extensions::UpdateObserver,
                              public system::AutomaticRebootManagerObserver,
                              public KioskAppManagerObserver {
 public:
  KioskAppUpdateService(
      Profile* profile,
      system::AutomaticRebootManager* automatic_reboot_manager);
  ~KioskAppUpdateService() override;

  void Init(const std::string& app_id);

  std::string get_app_id() const { return app_id_; }

 private:
  friend class KioskAppUpdateServiceTest;

  void StartAppUpdateRestartTimer();
  void ForceAppUpdateRestart();

  // KeyedService overrides:
  void Shutdown() override;

  // extensions::UpdateObserver overrides:
  void OnAppUpdateAvailable(const extensions::Extension* extension) override;
  void OnChromeUpdateAvailable() override {}

  // system::AutomaticRebootManagerObserver overrides:
  void OnRebootRequested(Reason reason) override;
  void WillDestroyAutomaticRebootManager() override;

  // KioskAppManagerObserver overrides:
  void OnKioskAppCacheUpdated(const std::string& app_id) override;

  Profile* profile_;
  std::string app_id_;

  // After we detect an upgrade we start a one-short timer to force restart.
  base::OneShotTimer restart_timer_;

  system::AutomaticRebootManager* automatic_reboot_manager_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(KioskAppUpdateService);
};

// Singleton that owns all KioskAppUpdateServices and associates them with
// profiles.
class KioskAppUpdateServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the KioskAppUpdateService for |profile|, creating it if it is not
  // yet created.
  static KioskAppUpdateService* GetForProfile(Profile* profile);

  // Returns the KioskAppUpdateServiceFactory instance.
  static KioskAppUpdateServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<KioskAppUpdateServiceFactory>;

  KioskAppUpdateServiceFactory();
  ~KioskAppUpdateServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_UPDATE_SERVICE_H_
