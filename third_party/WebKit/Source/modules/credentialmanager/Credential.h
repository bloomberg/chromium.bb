// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Credential_h
#define Credential_h

#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/credentialmanager/PlatformCredential.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"

namespace blink {

class ExceptionState;

class MODULES_EXPORT Credential : public GarbageCollectedFinalized<Credential>,
                                  public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual ~Credential();

  // Credential.idl
  const String& id() const { return platform_credential_->Id(); }
  const String& type() const { return platform_credential_->GetType(); }

  virtual void Trace(blink::Visitor*);

  PlatformCredential* GetPlatformCredential() const {
    return platform_credential_;
  }

 protected:
  Credential(PlatformCredential*);
  Credential(const String& id);

  // Parses a string as a KURL. Throws an exception via |exceptionState| if an
  // invalid URL is produced.
  static KURL ParseStringAsURL(const String&, ExceptionState&);

  Member<PlatformCredential> platform_credential_;
};

}  // namespace blink

#endif  // Credential_h
