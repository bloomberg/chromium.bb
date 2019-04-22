// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_DIAGNOSTICSD_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_DIAGNOSTICSD_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_web_request_service.h"
#include "chrome/services/diagnosticsd/public/mojom/diagnosticsd.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/buffer.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace chromeos {

// Establishes Mojo communication to the diagnosticsd daemon. The Mojo pipe gets
// bootstrapped via D-Bus, and the class takes care of waiting until the
// diagnosticsd D-Bus service gets started and of repeating the bootstrapping
// after the daemon gets restarted.
class DiagnosticsdBridge final
    : public diagnosticsd::mojom::DiagnosticsdClient {
 public:
  // Delegate class, allowing to stub out unwanted operations in unit tests.
  class Delegate {
   public:
    virtual ~Delegate();

    // Creates a Mojo invitation that requests the remote implementation of the
    // DiagnosticsdServiceFactory interface.
    // Returns |diagnosticsd_service_factory_mojo_ptr| - interface pointer that
    // points to the remote implementation of the interface,
    // |remote_endpoint_fd| - file descriptor of the remote endpoint to be sent.
    virtual void CreateDiagnosticsdServiceFactoryMojoInvitation(
        diagnosticsd::mojom::DiagnosticsdServiceFactoryPtr*
            diagnosticsd_service_factory_mojo_ptr,
        base::ScopedFD* remote_endpoint_fd) = 0;
  };

  // Returns the global singleton instance.
  static DiagnosticsdBridge* Get();

  static base::TimeDelta connection_attempt_interval_for_testing();
  static int max_connection_attempt_count_for_testing();

  explicit DiagnosticsdBridge(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  // For use in tests.
  DiagnosticsdBridge(
      std::unique_ptr<Delegate> delegate,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~DiagnosticsdBridge() override;

  // Sets the Wilco DTC configuration data, passed and owned by the
  // |DiagnosticsdManager| from the device policy.
  // The nullptr should be passed to clear it.
  void SetConfigurationData(const std::string* data);
  const std::string& GetConfigurationDataForTesting();

  // Mojo proxy to the DiagnosticsdService implementation in the diagnosticsd
  // daemon. Returns null when bootstrapping of Mojo connection hasn't started
  // yet. Note that, however, non-null is already returned before the
  // bootstrapping fully completes.
  diagnosticsd::mojom::DiagnosticsdServiceProxy*
  diagnosticsd_service_mojo_proxy() {
    return diagnosticsd_service_mojo_ptr_ ? diagnosticsd_service_mojo_ptr_.get()
                                          : nullptr;
  }

 private:
  // Starts waiting until the diagnosticsd D-Bus service becomes available (or
  // until this waiting fails).
  void WaitForDBusService();
  // Schedules a postponed execution of WaitForDBusService().
  void ScheduleWaitingForDBusService();
  // Called once waiting for the D-Bus service, started by WaitForDBusService(),
  // finishes.
  void OnWaitedForDBusService(bool service_is_available);
  // Triggers Mojo bootstrapping via a D-Bus to the diagnosticsd daemon.
  void BootstrapMojoConnection();
  // Called once the result of the D-Bus call, made from
  // BootstrapMojoConnection(), arrives.
  void OnBootstrappedMojoConnection(bool success);
  // Called once the GetService() Mojo request completes.
  void OnMojoGetServiceCompleted();
  // Called when Mojo signals a connection error.
  void OnMojoConnectionError();

  // diagnosticsd::mojom::DiagnosticsdClient overrides.
  void PerformWebRequest(
      diagnosticsd::mojom::DiagnosticsdWebRequestHttpMethod http_method,
      mojo::ScopedHandle url,
      std::vector<mojo::ScopedHandle> headers,
      mojo::ScopedHandle request_body,
      PerformWebRequestCallback callback) override;
  void SendDiagnosticsProcessorMessageToUi(
      mojo::ScopedHandle json_message,
      SendDiagnosticsProcessorMessageToUiCallback callback) override;
  void GetConfigurationData(GetConfigurationDataCallback callback) override;

  std::unique_ptr<Delegate> delegate_;

  // Mojo binding that binds |this| as an implementation of the
  // DiagnosticsdClient Mojo interface.
  mojo::Binding<diagnosticsd::mojom::DiagnosticsdClient> mojo_self_binding_{
      this};

  // Current consecutive connection attempt number.
  int connection_attempt_ = 0;

  // Interface pointers to the Mojo services exposed by the diagnosticsd daemon.
  diagnosticsd::mojom::DiagnosticsdServiceFactoryPtr
      diagnosticsd_service_factory_mojo_ptr_;
  diagnosticsd::mojom::DiagnosticsdServicePtr diagnosticsd_service_mojo_ptr_;

  // The service to perform diagnostics_processor's web requests.
  DiagnosticsdWebRequestService web_request_service_;

  // The Wilco DTC configuration data blob, passed from the device policy, is
  // stored and owned by |DiagnosticsdManager|.
  // nullptr if there is no available configuration data for the Wilco DTC.
  const std::string* configuration_data_ = nullptr;

  // These weak pointer factories must be the last members:

  // Used for cancelling previously posted tasks that wait for the D-Bus service
  // availability.
  base::WeakPtrFactory<DiagnosticsdBridge> dbus_waiting_weak_ptr_factory_{this};
  base::WeakPtrFactory<DiagnosticsdBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdBridge);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_DIAGNOSTICSD_BRIDGE_H_
