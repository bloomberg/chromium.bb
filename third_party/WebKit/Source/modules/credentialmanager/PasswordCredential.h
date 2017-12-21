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

class MODULES_EXPORT PasswordCredential final : public Credential {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PasswordCredential* Create(const PasswordCredentialData&,
                                    ExceptionState&);
  static PasswordCredential* Create(HTMLFormElement*, ExceptionState&);
  static PasswordCredential* Create(const String& id,
                                    const String& password,
                                    const String& name,
                                    const KURL& icon_url);

  // Credential:
  bool IsPasswordCredential() const override;

  // PasswordCredential.idl
  const String& password() const { return password_; }
  const String& name() const { return name_; }
  const KURL& iconURL() const { return icon_url_; }

 private:
  PasswordCredential(const String& id,
                     const String& password,
                     const String& name,
                     const KURL& icon);

  const String password_;
  const String name_;
  const KURL icon_url_;
};

}  // namespace blink

#endif  // PasswordCredential_h
