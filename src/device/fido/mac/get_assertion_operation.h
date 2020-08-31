// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_GET_ASSERTION_OPERATION_H_
#define DEVICE_FIDO_MAC_GET_ASSERTION_OPERATION_H_

#include <os/availability.h>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/operation.h"
#include "device/fido/mac/touch_id_context.h"

namespace device {
namespace fido {
namespace mac {

// GetAssertionOperation implements the authenticatorGetAssertion operation. The
// operation can be invoked via its |Run| method, which must only be called
// once.
//
// It prompts the user for consent via Touch ID, then looks up a key pair
// matching the request in the keychain and generates an assertion.
//
// For documentation on the keychain item metadata, see
// |MakeCredentialOperation|.
class API_AVAILABLE(macosx(10.12.2))
    COMPONENT_EXPORT(DEVICE_FIDO) GetAssertionOperation : public Operation {
 public:
  using Callback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      base::Optional<AuthenticatorGetAssertionResponse>)>;

  GetAssertionOperation(CtapGetAssertionRequest request,
                        TouchIdCredentialStore* credential_store,
                        Callback callback);
  ~GetAssertionOperation() override;

  // Operation:
  void Run() override;

  // GetNextAssertion() may be called for a request with an empty allowList
  // after the initial callback has returned.
  void GetNextAssertion(Callback callback);

 private:
  void PromptTouchIdDone(bool success);
  base::Optional<AuthenticatorGetAssertionResponse> ResponseForCredential(
      const Credential& credential);

  const std::unique_ptr<TouchIdContext> touch_id_context_ =
      TouchIdContext::Create();

  const CtapGetAssertionRequest request_;
  TouchIdCredentialStore* const credential_store_;
  Callback callback_;
  std::list<Credential> matching_credentials_;

  DISALLOW_COPY_AND_ASSIGN(GetAssertionOperation);
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_GET_ASSERTION_OPERATION_H_
