// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_DIAGNOSTICSD_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_DIAGNOSTICSD_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace chromeos {

class DiagnosticsdBridge;

// The class controls the lifetime of the wilco DTC (diagnostics and telemetry
// controller) support services.
// The wilco DTC is allowed to be running only when:
// * The wilco DTC is enabled by policy.
// * An affiliated user/no user is logged-in.
class DiagnosticsdManager final
    : public session_manager::SessionManagerObserver {
 public:
  using WilcoDtcCallback = base::OnceCallback<void(bool)>;

  // Delegate class, allowing to pass a stub diagnosticsd bridge in unit tests.
  class Delegate {
   public:
    virtual ~Delegate();
    // Returns a DiagnosticsdBridge instance.
    virtual std::unique_ptr<DiagnosticsdBridge> CreateDiagnosticsdBridge() = 0;
  };

  // Returns the global singleton instance.
  static DiagnosticsdManager* Get();

  DiagnosticsdManager();
  // For use in tests.
  explicit DiagnosticsdManager(std::unique_ptr<Delegate> delegate);

  ~DiagnosticsdManager() override;

  // Sets the Wilco DTC configuration data, passed by the device policy.
  // The nullptr should be passed to clear it.
  // Notifies the |diagnosticsd_bridge_| if it is created.
  void SetConfigurationData(std::unique_ptr<std::string> data);

 private:
  // session_manager::SessionManagerObserver override:
  void OnSessionStateChanged() override;

  // Makes a decision and starts/stops wilco DTC if necessary.
  void StartOrStopWilcoDtc();

  // Starts the wilco DTC. |callback| is called after the method call finishes.
  void StartWilcoDtc(WilcoDtcCallback callback);

  // Stops the wilco DTC. |callback| is called after the method call finishes.
  void StopWilcoDtc(WilcoDtcCallback callback);

  void OnStartWilcoDtc(bool success);
  void OnStopWilcoDtc(bool success);

  std::unique_ptr<Delegate> delegate_;

  // Observer to changes in the wilco DTC allowed policy.
  std::unique_ptr<CrosSettings::ObserverSubscription>
      wilco_dtc_allowed_observer_;

  // The configuration data blob is stored and owned.
  std::unique_ptr<std::string> configuration_data_;

  std::unique_ptr<DiagnosticsdBridge> diagnosticsd_bridge_;

  // |callback_weak_factory_ptr_| is used only in Stop/StartWilcoDtc to be able
  // to discard the callbacks for the older requests.
  base::WeakPtrFactory<DiagnosticsdManager> callback_weak_ptr_factory_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DiagnosticsdManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_DIAGNOSTICSD_MANAGER_H_
