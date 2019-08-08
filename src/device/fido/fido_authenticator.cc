// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_authenticator.h"

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "device/fido/fido_constants.h"
#include "device/fido/pin.h"

namespace device {

void FidoAuthenticator::GetNextAssertion(
    FidoAuthenticator::GetAssertionCallback callback) {
  NOTREACHED();
}

void FidoAuthenticator::GetTouch(base::OnceCallback<void()> callback) {}

void FidoAuthenticator::GetRetries(
    FidoAuthenticator::GetRetriesCallback callback) {
  NOTREACHED();
}

void FidoAuthenticator::GetEphemeralKey(
    FidoAuthenticator::GetEphemeralKeyCallback callback) {
  NOTREACHED();
}

void FidoAuthenticator::GetPINToken(
    std::string pin,
    const pin::KeyAgreementResponse& peer_key,
    FidoAuthenticator::GetPINTokenCallback callback) {
  NOTREACHED();
}

void FidoAuthenticator::SetPIN(const std::string& pin,
                               const pin::KeyAgreementResponse& peer_key,
                               FidoAuthenticator::SetPINCallback callback) {
  NOTREACHED();
}

void FidoAuthenticator::ChangePIN(const std::string& old_pin,
                                  const std::string& new_pin,
                                  pin::KeyAgreementResponse& peer_key,
                                  SetPINCallback callback) {
  NOTREACHED();
}

FidoAuthenticator::MakeCredentialPINDisposition
FidoAuthenticator::WillNeedPINToMakeCredential(
    const CtapMakeCredentialRequest& request,
    const FidoRequestHandlerBase::Observer* observer) {
  return MakeCredentialPINDisposition::kNoPIN;
}

FidoAuthenticator::GetAssertionPINDisposition
FidoAuthenticator::WillNeedPINToGetAssertion(
    const CtapGetAssertionRequest& request,
    const FidoRequestHandlerBase::Observer* observer) {
  return GetAssertionPINDisposition::kNoPIN;
}

void FidoAuthenticator::Reset(ResetCallback callback) {
  std::move(callback).Run(CtapDeviceResponseCode::kCtap1ErrInvalidCommand,
                          base::nullopt);
}

ProtocolVersion FidoAuthenticator::SupportedProtocol() const {
  return ProtocolVersion::kUnknown;
}

}  // namespace device
