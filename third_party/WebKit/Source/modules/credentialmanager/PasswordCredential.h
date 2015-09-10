// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PasswordCredential_h
#define PasswordCredential_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "modules/credentialmanager/Credential.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"

namespace blink {

class FormData;
class FormDataOptions;
class PasswordCredentialData;
class WebPasswordCredential;

class PasswordCredential final : public Credential {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PasswordCredential* create(const PasswordCredentialData&, ExceptionState&);
    static PasswordCredential* create(WebPasswordCredential*);

    // PasswordCredential.idl
    FormData* toFormData(ScriptState*, const FormDataOptions&);

    DECLARE_VIRTUAL_TRACE();

private:
    PasswordCredential(WebPasswordCredential*);
    PasswordCredential(const String& id, const String& password, const String& name, const KURL& icon);

    const String& password() const;
};

} // namespace blink

#endif // PasswordCredential_h
