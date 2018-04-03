// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/CredentialManagerProxy.h"

#include "core/dom/Document.h"
#include "core/execution_context/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "platform/bindings/ScriptState.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

CredentialManagerProxy::CredentialManagerProxy(Document* document) {
  LocalFrame* frame = document->GetFrame();
  DCHECK(frame);
  frame->GetInterfaceProvider().GetInterface(&credential_manager_);
  frame->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&authenticator_));
}

CredentialManagerProxy::~CredentialManagerProxy() {}

// static
CredentialManagerProxy* CredentialManagerProxy::From(Document* document) {
  DCHECK(document);
  auto* supplement =
      Supplement<Document>::From<CredentialManagerProxy>(document);
  if (!supplement) {
    supplement = new CredentialManagerProxy(document);
    ProvideTo(*document, supplement);
  }
  return supplement;
}

// static
CredentialManagerProxy* CredentialManagerProxy::From(
    ScriptState* script_state) {
  DCHECK(script_state->ContextIsValid());
  return From(ToDocumentOrNull(ExecutionContext::From(script_state)));
}

// static
const char CredentialManagerProxy::kSupplementName[] = "CredentialManagerProxy";

}  // namespace blink
