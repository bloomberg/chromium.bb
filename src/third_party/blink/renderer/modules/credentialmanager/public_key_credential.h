// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_PUBLIC_KEY_CREDENTIAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_PUBLIC_KEY_CREDENTIAL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-shared.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_outputs.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/credentialmanager/authenticator_response.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class AuthenticatorResponse;
class AuthenticatorTransport;
class ScriptPromise;
class ScriptState;

class MODULES_EXPORT PublicKeyCredential : public Credential {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PublicKeyCredential(
      const String& id,
      DOMArrayBuffer* raw_id,
      AuthenticatorResponse*,
      bool has_transport,
      mojom::AuthenticatorTransport transport,
      const AuthenticationExtensionsClientOutputs* extension_outputs,
      const String& type = "");

  DOMArrayBuffer* rawId() const { return raw_id_.Get(); }
  AuthenticatorResponse* response() const { return response_.Get(); }
  absl::optional<String> authenticatorAttachment() const {
    return authenticator_attachment_;
  }
  static ScriptPromise isUserVerifyingPlatformAuthenticatorAvailable(
      ScriptState*);
  AuthenticationExtensionsClientOutputs* getClientExtensionResults() const;

  // Credential:
  void Trace(Visitor*) const override;
  bool IsPublicKeyCredential() const override;

 private:
  const Member<DOMArrayBuffer> raw_id_;
  const Member<AuthenticatorResponse> response_;
  const absl::optional<String> authenticator_attachment_;
  Member<const AuthenticationExtensionsClientOutputs> extension_outputs_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_PUBLIC_KEY_CREDENTIAL_H_
