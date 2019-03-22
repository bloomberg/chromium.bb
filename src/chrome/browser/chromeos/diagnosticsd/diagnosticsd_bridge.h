// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_DIAGNOSTICSD_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_DIAGNOSTICSD_BRIDGE_H_

#include <memory>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/services/diagnosticsd/public/mojom/diagnosticsd.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

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

  DiagnosticsdBridge();
  // For use in tests.
  explicit DiagnosticsdBridge(std::unique_ptr<Delegate> delegate);

  ~DiagnosticsdBridge() override;

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

  // These weak pointer factories must be the last members:

  // Used for cancelling previously posted tasks that wait for the D-Bus service
  // availability.
  base::WeakPtrFactory<DiagnosticsdBridge> dbus_waiting_weak_ptr_factory_{this};
  base::WeakPtrFactory<DiagnosticsdBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdBridge);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DIAGNOSTICSD_DIAGNOSTICSD_BRIDGE_H_
