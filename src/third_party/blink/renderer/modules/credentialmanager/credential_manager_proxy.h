// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIAL_MANAGER_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIAL_MANAGER_PROXY_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/credentialmanager/credential_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom-blink.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ScriptState;

// Owns the client end of the mojo::CredentialManager interface connection to an
// implementation that services requests in the security context of the window
// supplemented by this CredentialManagerProxy instance.
//
// This facilitates routing API calls to be serviced in the correct security
// context, even if the `window.navigator.credentials` instance from one
// browsing context was passed to another; in which case the Credential
// Management API call must still be serviced in the browsing context
// responsible for actually calling the API method, as opposed to the context
// whose global object owns the CredentialsContainer instance on which the
// method was called.
class MODULES_EXPORT CredentialManagerProxy
    : public GarbageCollected<CredentialManagerProxy>,
      public Supplement<LocalDOMWindow> {
  USING_GARBAGE_COLLECTED_MIXIN(CredentialManagerProxy);

 public:
  static const char kSupplementName[];

  explicit CredentialManagerProxy(LocalDOMWindow&);
  virtual ~CredentialManagerProxy();

  mojom::blink::CredentialManager* CredentialManager() {
    return credential_manager_.get();
  }

  mojom::blink::Authenticator* Authenticator() { return authenticator_.get(); }

  mojom::blink::SmsReceiver* SmsReceiver();

  void FlushCredentialManagerConnectionForTesting() {
    credential_manager_.FlushForTesting();
  }

  // Must be called only with argument representing a valid
  // context corresponding to an attached window.
  static CredentialManagerProxy* From(ScriptState*);

 private:
  mojo::Remote<mojom::blink::Authenticator> authenticator_;
  mojo::Remote<mojom::blink::CredentialManager> credential_manager_;
  mojo::Remote<mojom::blink::SmsReceiver> sms_receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIAL_MANAGER_PROXY_H_
