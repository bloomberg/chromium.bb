// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/diagnosticsd_client.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void OnVoidDBusMethod(VoidDBusMethodCallback callback,
                      dbus::Response* response) {
  std::move(callback).Run(response != nullptr);
}

// The DiagnosticsdClient implementation used in production.
class DiagnosticsdClientImpl final : public DiagnosticsdClient {
 public:
  DiagnosticsdClientImpl();
  ~DiagnosticsdClientImpl() override;

  // DiagnosticsdClient overrides:
  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override;
  void BootstrapMojoConnection(base::ScopedFD fd,
                               VoidDBusMethodCallback callback) override;

 protected:
  // DiagnosticsdClient overrides:
  void Init(dbus::Bus* bus) override;

 private:
  dbus::ObjectProxy* diagnosticsd_proxy_ = nullptr;

  base::WeakPtrFactory<DiagnosticsdClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdClientImpl);
};

DiagnosticsdClientImpl::DiagnosticsdClientImpl() = default;

DiagnosticsdClientImpl::~DiagnosticsdClientImpl() = default;

void DiagnosticsdClientImpl::WaitForServiceToBeAvailable(
    WaitForServiceToBeAvailableCallback callback) {
  diagnosticsd_proxy_->WaitForServiceToBeAvailable(std::move(callback));
}

void DiagnosticsdClientImpl::BootstrapMojoConnection(
    base::ScopedFD fd,
    VoidDBusMethodCallback callback) {
  dbus::MethodCall method_call(
      ::diagnostics::kDiagnosticsdServiceInterface,
      ::diagnostics::kDiagnosticsdBootstrapMojoConnectionMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendFileDescriptor(fd.get());
  diagnosticsd_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnVoidDBusMethod, std::move(callback)));
}

void DiagnosticsdClientImpl::Init(dbus::Bus* bus) {
  diagnosticsd_proxy_ = bus->GetObjectProxy(
      ::diagnostics::kDiagnosticsdServiceName,
      dbus::ObjectPath(::diagnostics::kDiagnosticsdServicePath));
}

}  // namespace

// static
std::unique_ptr<DiagnosticsdClient> DiagnosticsdClient::Create() {
  return std::make_unique<DiagnosticsdClientImpl>();
}

DiagnosticsdClient::DiagnosticsdClient() = default;

}  // namespace chromeos
