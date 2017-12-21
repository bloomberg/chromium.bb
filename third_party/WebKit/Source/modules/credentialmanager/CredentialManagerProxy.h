// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CredentialManagerProxy_h
#define CredentialManagerProxy_h

#include "core/dom/Document.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/credentialmanager/credential_manager.mojom-blink.h"
#include "public/platform/modules/webauth/authenticator.mojom-blink.h"

namespace blink {

class ScriptState;

// Owns the client end of the mojo::CredentialManager interface connection to an
// implementation that services requests in the security context of the document
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
    : public GarbageCollectedFinalized<CredentialManagerProxy>,
      public Supplement<Document> {
  USING_GARBAGE_COLLECTED_MIXIN(CredentialManagerProxy);

 public:
  explicit CredentialManagerProxy(Document*);
  virtual ~CredentialManagerProxy();

  ::password_manager::mojom::blink::CredentialManager* CredentialManager() {
    return credential_manager_.get();
  }

  ::webauth::mojom::blink::Authenticator* Authenticator() {
    return authenticator_.get();
  }

  void FlushCredentialManagerConnectionForTesting() {
    credential_manager_.FlushForTesting();
  }

  // Both flavors must be called only with arguments representing a valid
  // context corresponding to an attached Document.
  static CredentialManagerProxy* From(ScriptState*);
  static CredentialManagerProxy* From(Document*);

 private:
  static const char* SupplementName();

  // TODO(crbug.com/740081): Merge |credential_manager_| and |authenticator_|
  // into a single Mojo interface.
  ::password_manager::mojom::blink::CredentialManagerPtr credential_manager_;
  ::webauth::mojom::blink::AuthenticatorPtr authenticator_;
};

}  // namespace blink

#endif  // CredentialManagerProxy_h
