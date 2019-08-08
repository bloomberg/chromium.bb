// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/bio/enrollment_handler.h"

#include "base/bind.h"
#include "device/fido/bio/enrollment.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"

namespace device {

BioEnrollmentHandler::BioEnrollmentHandler(
    service_manager::Connector* connector,
    FidoDiscoveryFactory* factory,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    base::OnceClosure ready_callback)
    : FidoRequestHandlerBase(connector, factory, supported_transports),
      ready_callback_(std::move(ready_callback)),
      weak_factory_(this) {
  Start();
}

BioEnrollmentHandler::~BioEnrollmentHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
}

void BioEnrollmentHandler::GetModality(GetInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);

  if (authenticator_->Options()->bio_enrollment_availability_preview ==
      AuthenticatorSupportedOptions::BioEnrollmentAvailability::kNotSupported) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrUnsupportedOption,
                            base::nullopt);
  } else {
    callback_ = std::move(callback);
    authenticator_->GetModality(base::BindOnce(&BioEnrollmentHandler::OnGetInfo,
                                               weak_factory_.GetWeakPtr()));
  }
}

void BioEnrollmentHandler::GetSensorInfo(GetInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);

  if (authenticator_->Options()->bio_enrollment_availability_preview ==
      AuthenticatorSupportedOptions::BioEnrollmentAvailability::kNotSupported) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrUnsupportedOption,
                            base::nullopt);
  } else {
    callback_ = std::move(callback);
    authenticator_->GetSensorInfo(base::BindOnce(
        &BioEnrollmentHandler::OnGetInfo, weak_factory_.GetWeakPtr()));
  }
}

void BioEnrollmentHandler::DispatchRequest(FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
  authenticator->GetTouch(base::BindOnce(&BioEnrollmentHandler::OnTouch,
                                         weak_factory_.GetWeakPtr(),
                                         authenticator));
}

void BioEnrollmentHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  if (authenticator_ == authenticator) {
    authenticator_ = nullptr;
  }
  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);
}

void BioEnrollmentHandler::OnTouch(FidoAuthenticator* authenticator) {
  authenticator_ = authenticator;
  CancelActiveAuthenticators(authenticator_->GetId());
  std::move(ready_callback_).Run();
}

void BioEnrollmentHandler::OnGetInfo(
    CtapDeviceResponseCode status,
    base::Optional<BioEnrollmentResponse> response) {
  std::move(callback_).Run(status, response);
}

}  // namespace device
