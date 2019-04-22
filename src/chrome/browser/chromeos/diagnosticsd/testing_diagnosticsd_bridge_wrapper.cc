// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/diagnosticsd/testing_diagnosticsd_bridge_wrapper.h"

#include <unistd.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/diagnosticsd_client.h"
#include "chromeos/dbus/fake_diagnosticsd_client.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace chromeos {

namespace {

// Testing implementation of the DiagnosticsdServiceFactory Mojo service that
// allows to stub out the GetService Mojo method and tie it with the testing
// implementation instead.
class TestingMojoDiagnosticsdServiceFactory final
    : public diagnosticsd::mojom::DiagnosticsdServiceFactory {
 public:
  // |get_service_handler_callback| is the callback that will be run when
  // GetService() is called.
  explicit TestingMojoDiagnosticsdServiceFactory(
      base::RepeatingCallback<void(
          diagnosticsd::mojom::DiagnosticsdServiceRequest
              mojo_diagnosticsd_service_request,
          diagnosticsd::mojom::DiagnosticsdClientPtr mojo_diagnosticsd_client)>
          get_service_handler_callback)
      : get_service_handler_callback_(std::move(get_service_handler_callback)) {
  }

  // Completes the Mojo binding of |this| to the given Mojo interface request.
  // This method allows to redirect to |this| the calls on the
  // DiagnosticsdServiceFactory interface that are made by the
  // DiagnosticsdBridge.
  void Bind(diagnosticsd::mojom::DiagnosticsdServiceFactoryRequest
                mojo_diagnosticsd_service_factory_request) {
    // First close the Mojo binding in case it was previously completed, to
    // allow calling this method multiple times.
    self_binding_.Close();
    self_binding_.Bind(std::move(mojo_diagnosticsd_service_factory_request));
  }

  // DiagnosticsdServiceFactory overrides:

  void GetService(diagnosticsd::mojom::DiagnosticsdServiceRequest service,
                  diagnosticsd::mojom::DiagnosticsdClientPtr client,
                  GetServiceCallback callback) override {
    DCHECK(service);
    DCHECK(client);
    // Redirect to |get_service_handler_callback_| to let
    // TestingDiagnosticsdBridgeWrapper capture |client| (which points to the
    // production implementation in DiagnosticsdBridge) and fulfill |service|
    // (to make it point to the stub implementation of the DiagnosticsdService
    // Mojo service that was passed to TestingDiagnosticsdBridgeWrapper).
    get_service_handler_callback_.Run(std::move(service), std::move(client));
    std::move(callback).Run();
  }

 private:
  // Mojo binding that binds |this| as an implementation of the
  // DiagnosticsdClient Mojo interface.
  mojo::Binding<diagnosticsd::mojom::DiagnosticsdServiceFactory> self_binding_{
      this};
  // The callback to be run when GetService() is called.
  base::RepeatingCallback<void(
      diagnosticsd::mojom::DiagnosticsdServiceRequest
          mojo_diagnosticsd_service_request,
      diagnosticsd::mojom::DiagnosticsdClientPtr mojo_diagnosticsd_client)>
      get_service_handler_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestingMojoDiagnosticsdServiceFactory);
};

// Testing implementation of the DiagnosticsdBridge delegate that stubs out the
// process of generating the Mojo invitation and tie it with
// TestingMojoDiagnosticsdServiceFactory instead.
class TestingDiagnosticsdBridgeWrapperDelegate final
    : public DiagnosticsdBridge::Delegate {
 public:
  explicit TestingDiagnosticsdBridgeWrapperDelegate(
      std::unique_ptr<TestingMojoDiagnosticsdServiceFactory>
          mojo_diagnosticsd_service_factory)
      : mojo_diagnosticsd_service_factory_(
            std::move(mojo_diagnosticsd_service_factory)) {}

  // DiagnosticsdBridge::Delegate overrides:

  void CreateDiagnosticsdServiceFactoryMojoInvitation(
      diagnosticsd::mojom::DiagnosticsdServiceFactoryPtr*
          diagnosticsd_service_factory_mojo_ptr,
      base::ScopedFD* remote_endpoint_fd) override {
    // Bind the Mojo pointer passed to the bridge with the
    // TestingMojoDiagnosticsdServiceFactory implementation.
    mojo_diagnosticsd_service_factory_->Bind(
        mojo::MakeRequest(diagnosticsd_service_factory_mojo_ptr));

    // Return a fake file descriptor - its value is not used in the unit test
    // environment for anything except comparing with zero.
    remote_endpoint_fd->reset(HANDLE_EINTR(dup(STDIN_FILENO)));
    DCHECK(remote_endpoint_fd->is_valid());
  }

 private:
  std::unique_ptr<TestingMojoDiagnosticsdServiceFactory>
      mojo_diagnosticsd_service_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestingDiagnosticsdBridgeWrapperDelegate);
};

FakeDiagnosticsdClient* GetFakeDbusDiagnosticsdClient() {
  DCHECK(DBusThreadManager::Get()->IsUsingFakes());
  DiagnosticsdClient* const diagnosticsd_client =
      DBusThreadManager::Get()->GetDiagnosticsdClient();
  DCHECK(diagnosticsd_client);
  return static_cast<FakeDiagnosticsdClient*>(diagnosticsd_client);
}

}  // namespace

// static
std::unique_ptr<TestingDiagnosticsdBridgeWrapper>
TestingDiagnosticsdBridgeWrapper::Create(
    diagnosticsd::mojom::DiagnosticsdService* mojo_diagnosticsd_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<DiagnosticsdBridge>* bridge) {
  return base::WrapUnique(new TestingDiagnosticsdBridgeWrapper(
      mojo_diagnosticsd_service, std::move(url_loader_factory), bridge));
}

TestingDiagnosticsdBridgeWrapper::~TestingDiagnosticsdBridgeWrapper() = default;

void TestingDiagnosticsdBridgeWrapper::EstablishFakeMojoConnection() {
  DCHECK(!mojo_diagnosticsd_client_);
  DCHECK(!mojo_get_service_handler_);

  // Set up the callback that will handle the GetService Mojo method called
  // during the bootstrap.
  base::RunLoop run_loop;
  diagnosticsd::mojom::DiagnosticsdServiceRequest
      intercepted_mojo_diagnosticsd_service_request;
  mojo_get_service_handler_ = base::BindLambdaForTesting(
      [&run_loop, &intercepted_mojo_diagnosticsd_service_request](
          diagnosticsd::mojom::DiagnosticsdServiceRequest
              mojo_diagnosticsd_service_request) {
        intercepted_mojo_diagnosticsd_service_request =
            std::move(mojo_diagnosticsd_service_request);
        run_loop.Quit();
      });

  // Trigger the Mojo bootstrapping process by unblocking the corresponding
  // D-Bus operations.
  FakeDiagnosticsdClient* const fake_dbus_diagnosticsd_client =
      GetFakeDbusDiagnosticsdClient();
  fake_dbus_diagnosticsd_client->SetWaitForServiceToBeAvailableResult(true);
  fake_dbus_diagnosticsd_client->SetBootstrapMojoConnectionResult(true);

  // Wait till the GetService Mojo method call completes.
  run_loop.Run();
  DCHECK(intercepted_mojo_diagnosticsd_service_request);
  DCHECK(mojo_diagnosticsd_client_);

  // First close the Mojo binding in case it was previously completed, to allow
  // calling this method multiple times.
  mojo_diagnosticsd_service_binding_.Close();
  mojo_diagnosticsd_service_binding_.Bind(
      std::move(intercepted_mojo_diagnosticsd_service_request));
}

void TestingDiagnosticsdBridgeWrapper::HandleMojoGetService(
    diagnosticsd::mojom::DiagnosticsdServiceRequest
        mojo_diagnosticsd_service_request,
    diagnosticsd::mojom::DiagnosticsdClientPtr mojo_diagnosticsd_client) {
  std::move(mojo_get_service_handler_)
      .Run(std::move(mojo_diagnosticsd_service_request));
  mojo_diagnosticsd_client_ = std::move(mojo_diagnosticsd_client);
}

TestingDiagnosticsdBridgeWrapper::TestingDiagnosticsdBridgeWrapper(
    diagnosticsd::mojom::DiagnosticsdService* mojo_diagnosticsd_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<DiagnosticsdBridge>* bridge)
    : mojo_diagnosticsd_service_binding_(mojo_diagnosticsd_service) {
  *bridge = std::make_unique<DiagnosticsdBridge>(
      std::make_unique<TestingDiagnosticsdBridgeWrapperDelegate>(
          std::make_unique<TestingMojoDiagnosticsdServiceFactory>(
              base::BindRepeating(
                  &TestingDiagnosticsdBridgeWrapper::HandleMojoGetService,
                  base::Unretained(this)))),
      url_loader_factory);
}

}  // namespace chromeos
