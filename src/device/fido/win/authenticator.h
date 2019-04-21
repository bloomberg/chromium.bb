// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_AUTHENTICATOR_H_
#define DEVICE_FIDO_WIN_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/win/webauthn_api.h"

namespace device {

// WinWebAuthnApiAuthenticator forwards WebAuthn requests to external
// authenticators via the native Windows WebAuthentication API
// (webauthn.dll).
//
// Callers must ensure that WinWebAuthnApi::IsAvailable() returns true
// before creating instances of this class.
class COMPONENT_EXPORT(DEVICE_FIDO) WinWebAuthnApiAuthenticator
    : public FidoAuthenticator {
 public:
  // The return value of |GetId|.
  static const char kAuthenticatorId[];

  // This method is safe to call without checking
  // WinWebAuthnApi::IsAvailable().
  static bool IsUserVerifyingPlatformAuthenticatorAvailable();

  // Instantiates an authenticator that uses the default WinWebAuthnApi.
  //
  // Callers must ensure that WinWebAuthnApi::IsAvailable() returns true
  // before creating instances of this class.
  WinWebAuthnApiAuthenticator(HWND current_window);
  ~WinWebAuthnApiAuthenticator() override;

  // FidoAuthenticator
  void InitializeAuthenticator(base::OnceClosure callback) override;
  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialCallback callback) override;
  void GetAssertion(CtapGetAssertionRequest request,
                    GetAssertionCallback callback) override;
  void Cancel() override;
  std::string GetId() const override;
  base::string16 GetDisplayName() const override;
  bool IsInPairingMode() const override;
  bool IsPaired() const override;
  const base::Optional<AuthenticatorSupportedOptions>& Options() const override;
  base::Optional<FidoTransportProtocol> AuthenticatorTransport() const override;
  bool IsWinNativeApiAuthenticator() const override;
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

 private:
  void MakeCredentialDone(
      MakeCredentialCallback callback,
      std::pair<CtapDeviceResponseCode,
                base::Optional<AuthenticatorMakeCredentialResponse>> result);
  void GetAssertionDone(
      GetAssertionCallback callback,
      std::pair<CtapDeviceResponseCode,
                base::Optional<AuthenticatorGetAssertionResponse>> result);

  HWND current_window_;

  bool is_pending_ = false;
  bool waiting_for_cancellation_ = false;
  GUID cancellation_id_ = {};
  // The pointee of |win_api_| is assumed to be a singleton that outlives
  // this instance.
  WinWebAuthnApi* win_api_;

  // Verifies callbacks from |win_api_| are posted back onto the originating
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WinWebAuthnApiAuthenticator> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(WinWebAuthnApiAuthenticator);
};

}  // namespace device

#endif  // DEVICE_FIDO_WIN_AUTHENTICATOR_H_
