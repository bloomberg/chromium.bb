// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace machine_learning {

namespace {

// Real Impl of ServiceConnection
class ServiceConnectionImpl : public ServiceConnection {
 public:
  ServiceConnectionImpl();
  ~ServiceConnectionImpl() override = default;

  void LoadModel(mojom::ModelSpecPtr spec,
                 mojom::ModelRequest request,
                 mojom::MachineLearningService::LoadModelCallback
                     result_callback) override;

 private:
  // Binds the top level interface |machine_learning_service_| to an
  // implementation in the ML Service daemon, if it is not already bound. The
  // binding is accomplished via D-Bus bootstrap.
  void BindMachineLearningServiceIfNeeded();

  // Mojo connection error handler. Resets |machine_learning_service_|, which
  // will be reconnected upon next use.
  void OnConnectionError();

  // Response callback for MlClient::BootstrapMojoConnection.
  void OnBootstrapMojoConnectionResponse(bool success);

  mojom::MachineLearningServicePtr machine_learning_service_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ServiceConnectionImpl);
};

void ServiceConnectionImpl::LoadModel(
    mojom::ModelSpecPtr spec,
    mojom::ModelRequest request,
    mojom::MachineLearningService::LoadModelCallback result_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindMachineLearningServiceIfNeeded();
  machine_learning_service_->LoadModel(std::move(spec), std::move(request),
                                       std::move(result_callback));
}

void ServiceConnectionImpl::BindMachineLearningServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (machine_learning_service_) {
    return;
  }

  mojo::PlatformChannel platform_channel;

  // Prepare a Mojo invitation to send through |platform_channel|.
  mojo::OutgoingInvitation invitation;
  // Include an initial Mojo pipe in the invitation.
  mojo::ScopedMessagePipeHandle pipe =
      invitation.AttachMessagePipe(ml::kBootstrapMojoConnectionChannelToken);
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 platform_channel.TakeLocalEndpoint());

  // Bind our end of |pipe| to our MachineLearningServicePtr. The daemon should
  // bind its end to a MachineLearningService implementation.
  machine_learning_service_.Bind(
      machine_learning::mojom::MachineLearningServicePtrInfo(std::move(pipe),
                                                             0u /* version */));
  machine_learning_service_.set_connection_error_handler(base::BindOnce(
      &ServiceConnectionImpl::OnConnectionError, base::Unretained(this)));

  // Send the file descriptor for the other end of |platform_channel| to the
  // ML service daemon over D-Bus.
  MachineLearningClient::Get()->BootstrapMojoConnection(
      platform_channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD(),
      base::BindOnce(&ServiceConnectionImpl::OnBootstrapMojoConnectionResponse,
                     base::Unretained(this)));
}

ServiceConnectionImpl::ServiceConnectionImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void ServiceConnectionImpl::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Connection errors are not expected so log a warning.
  LOG(WARNING) << "ML Service Mojo connection closed";
  machine_learning_service_.reset();
}

void ServiceConnectionImpl::OnBootstrapMojoConnectionResponse(
    const bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    LOG(WARNING) << "BootstrapMojoConnection D-Bus call failed";
    machine_learning_service_.reset();
  }
}

static ServiceConnection* g_fake_service_connection_for_testing = nullptr;

}  // namespace

ServiceConnection* ServiceConnection::GetInstance() {
  if (g_fake_service_connection_for_testing) {
    return g_fake_service_connection_for_testing;
  }
  static base::NoDestructor<ServiceConnectionImpl> service_connection;
  return service_connection.get();
}

void ServiceConnection::UseFakeServiceConnectionForTesting(
    ServiceConnection* const fake_service_connection) {
  g_fake_service_connection_for_testing = fake_service_connection;
}

}  // namespace machine_learning
}  // namespace chromeos
