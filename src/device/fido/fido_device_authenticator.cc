// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device_authenticator.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_device.h"
#include "device/fido/get_assertion_task.h"
#include "device/fido/make_credential_task.h"

namespace device {

FidoDeviceAuthenticator::FidoDeviceAuthenticator(
    std::unique_ptr<FidoDevice> device)
    : device_(std::move(device)), weak_factory_(this) {}
FidoDeviceAuthenticator::~FidoDeviceAuthenticator() = default;

void FidoDeviceAuthenticator::InitializeAuthenticator(
    base::OnceClosure callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FidoDevice::DiscoverSupportedProtocolAndDeviceInfo,
          device()->GetWeakPtr(),
          base::BindOnce(&FidoDeviceAuthenticator::InitializeAuthenticatorDone,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

void FidoDeviceAuthenticator::InitializeAuthenticatorDone(
    base::OnceClosure callback) {
  DCHECK(!options_);
  switch (device_->supported_protocol()) {
    case ProtocolVersion::kU2f:
      options_ = AuthenticatorSupportedOptions();
      break;
    case ProtocolVersion::kCtap:
      DCHECK(device_->device_info()) << "uninitialized device";
      options_ = device_->device_info()->options();
      break;
    case ProtocolVersion::kUnknown:
      NOTREACHED() << "uninitialized device";
      options_ = AuthenticatorSupportedOptions();
  }
  std::move(callback).Run();
}

void FidoDeviceAuthenticator::MakeCredential(CtapMakeCredentialRequest request,
                                             MakeCredentialCallback callback) {
  DCHECK(!task_);
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  DCHECK(Options());

  // Update the request to the "effective" user verification requirement.
  // https://w3c.github.io/webauthn/#effective-user-verification-requirement-for-credential-creation
  if (Options()->user_verification_availability() ==
      AuthenticatorSupportedOptions::UserVerificationAvailability::
          kSupportedAndConfigured) {
    request.SetUserVerification(UserVerificationRequirement::kRequired);
  } else {
    request.SetUserVerification(UserVerificationRequirement::kDiscouraged);
  }

  // TODO(martinkr): Change FidoTasks to take all request parameters by const
  // reference, so we can avoid copying these from the RequestHandler.
  task_ = std::make_unique<MakeCredentialTask>(
      device_.get(), std::move(request), std::move(callback));
}

void FidoDeviceAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                           GetAssertionCallback callback) {
  DCHECK(!task_);
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  DCHECK(Options());

  // Update the request to the "effective" user verification requirement.
  // https://w3c.github.io/webauthn/#effective-user-verification-requirement-for-assertion
  if (Options()->user_verification_availability() ==
      AuthenticatorSupportedOptions::UserVerificationAvailability::
          kSupportedAndConfigured) {
    request.SetUserVerification(UserVerificationRequirement::kRequired);
  } else {
    request.SetUserVerification(UserVerificationRequirement::kDiscouraged);
  }

  task_ = std::make_unique<GetAssertionTask>(device_.get(), std::move(request),
                                             std::move(callback));
}

void FidoDeviceAuthenticator::Cancel() {
  if (!task_)
    return;

  task_->CancelTask();
}

std::string FidoDeviceAuthenticator::GetId() const {
  return device_->GetId();
}

base::string16 FidoDeviceAuthenticator::GetDisplayName() const {
  return device_->GetDisplayName();
}

const base::Optional<AuthenticatorSupportedOptions>&
FidoDeviceAuthenticator::Options() const {
  return options_;
}

base::Optional<FidoTransportProtocol>
FidoDeviceAuthenticator::AuthenticatorTransport() const {
  return device_->DeviceTransport();
}

bool FidoDeviceAuthenticator::IsInPairingMode() const {
  return device_->IsInPairingMode();
}

bool FidoDeviceAuthenticator::IsPaired() const {
  return device_->IsPaired();
}

void FidoDeviceAuthenticator::SetTaskForTesting(
    std::unique_ptr<FidoTask> task) {
  task_ = std::move(task);
}

base::WeakPtr<FidoAuthenticator> FidoDeviceAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
