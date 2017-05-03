// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CredentialUserData_h
#define CredentialUserData_h

#include "modules/ModulesExport.h"
#include "modules/credentialmanager/Credential.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class MODULES_EXPORT CredentialUserData : public Credential {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // CredentialUserData.idl
  const String& name() const { return platform_credential_->GetName(); }
  const KURL& iconURL() const { return platform_credential_->GetIconURL(); }

 protected:
  CredentialUserData(PlatformCredential*);
};

}  // namespace blink

#endif  // CredentialUserData_h
