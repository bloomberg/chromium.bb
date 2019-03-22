// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_DIAGNOSTICSD_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_DIAGNOSTICSD_CLIENT_H_

#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/diagnosticsd_client.h"

namespace chromeos {

class CHROMEOS_EXPORT FakeDiagnosticsdClient final : public DiagnosticsdClient {
 public:
  FakeDiagnosticsdClient();
  ~FakeDiagnosticsdClient() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // DiagnosticsdClient overrides:
  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override;
  void BootstrapMojoConnection(base::ScopedFD fd,
                               VoidDBusMethodCallback callback) override;

  // Whether there's a pending WaitForServiceToBeAvailable call.
  int wait_for_service_to_be_available_in_flight_call_count() const;
  // If the passed optional is non-empty, then it determines the result for
  // pending and future WaitForServiceToBeAvailable calls. Otherwise, the
  // requests will stay pending.
  void SetWaitForServiceToBeAvailableResult(
      base::Optional<bool> wait_for_service_to_be_available_result);

  // Whether there's a pending BootstrapMojoConnection call.
  int bootstrap_mojo_connection_in_flight_call_count() const;
  // If the passed optional is non-empty, then it determines the result for
  // pending and future BootstrapMojoConnection calls. Otherwise, the requests
  // will stay pending.
  void SetBootstrapMojoConnectionResult(
      base::Optional<bool> bootstrap_mojo_connection_result);

 private:
  base::Optional<bool> wait_for_service_to_be_available_result_;
  std::vector<WaitForServiceToBeAvailableCallback>
      pending_wait_for_service_to_be_available_callbacks_;

  base::Optional<bool> bootstrap_mojo_connection_result_;
  std::vector<VoidDBusMethodCallback>
      pending_bootstrap_mojo_connection_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(FakeDiagnosticsdClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_DIAGNOSTICSD_CLIENT_H_
