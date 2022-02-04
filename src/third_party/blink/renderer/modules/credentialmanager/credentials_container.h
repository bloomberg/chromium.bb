// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIALS_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIALS_CONTAINER_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Credential;
class CredentialCreationOptions;
class CredentialRequestOptions;
class ExceptionState;
class Navigator;
class ScriptPromise;
class ScriptState;

class MODULES_EXPORT CredentialsContainer final : public ScriptWrappable,
                                                  public Supplement<Navigator> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static CredentialsContainer* credentials(Navigator&);
  explicit CredentialsContainer(Navigator&);

  // CredentialsContainer.idl
  ScriptPromise get(ScriptState*, const CredentialRequestOptions*);
  ScriptPromise store(ScriptState*, Credential* = nullptr);
  ScriptPromise create(ScriptState*,
                       const CredentialCreationOptions*,
                       ExceptionState&);
  ScriptPromise preventSilentAccess(ScriptState*);
  bool conditionalMediationSupported();

  void Trace(Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIALS_CONTAINER_H_
