// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

#include "base/no_destructor.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/machine_learning_client.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace machine_learning {

/* static */
ServiceConnection* ServiceConnection::GetInstance() {
  static base::NoDestructor<ServiceConnection> service_connection;
  return service_connection.get();
}

void ServiceConnection::LoadModel(
    mojom::ModelSpecPtr spec,
    mojom::ModelRequest request,
    mojom::MachineLearningService::LoadModelCallback result_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindMachineLearningServiceIfNeeded();
  machine_learning_service_->LoadModel(std::move(spec), std::move(request),
                                       std::move(result_callback));
}

void ServiceConnection::BindMachineLearningServiceIfNeeded() {
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
      &ServiceConnection::OnConnectionError, base::Unretained(this)));

  // Send the file descriptor for the other end of |platform_channel| to the
  // ML service daemon over D-Bus.
  DBusThreadManager::Get()->GetMachineLearningClient()->BootstrapMojoConnection(
      platform_channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD(),
      base::BindOnce(&ServiceConnection::OnBootstrapMojoConnectionResponse,
                     base::Unretained(this)));
}

ServiceConnection::ServiceConnection() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ServiceConnection::~ServiceConnection() {}

void ServiceConnection::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Connection errors are not expected so log a warning.
  LOG(WARNING) << "ML Service Mojo connection closed";
  machine_learning_service_.reset();
}

void ServiceConnection::OnBootstrapMojoConnectionResponse(const bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    LOG(WARNING) << "BootstrapMojoConnection D-Bus call failed";
    machine_learning_service_.reset();
  }
}

}  // namespace machine_learning
}  // namespace chromeos
