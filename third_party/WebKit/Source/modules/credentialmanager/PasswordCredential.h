// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PasswordCredential_h
#define PasswordCredential_h

#include "modules/ModulesExport.h"
#include "modules/credentialmanager/Credential.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"

namespace blink {

class HTMLFormElement;
class PasswordCredentialData;
class WebPasswordCredential;

class MODULES_EXPORT PasswordCredential final : public Credential {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PasswordCredential* Create(const PasswordCredentialData&,
                                    ExceptionState&);
  static PasswordCredential* Create(HTMLFormElement*, ExceptionState&);
  static PasswordCredential* Create(WebPasswordCredential*);

  // PasswordCredential.idl
  const String& password() const;
  const String& name() const;
  const KURL& iconURL() const;

 private:
  PasswordCredential(WebPasswordCredential*);
  PasswordCredential(const String& id,
                     const String& password,
                     const String& name,
                     const KURL& icon);
};

}  // namespace blink

#endif  // PasswordCredential_h
