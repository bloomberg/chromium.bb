// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LocalCredential_h
#define LocalCredential_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "modules/credentialmanager/Credential.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"

namespace blink {

class WebLocalCredential;

class LocalCredential final : public Credential {
    DEFINE_WRAPPERTYPEINFO();
public:
    static LocalCredential* create(WebLocalCredential*);
    static LocalCredential* create(const String& id, const String& name, const String& avatar, const String& password, ExceptionState&);

    // LocalCredential.idl
    const String& password() const;

private:
    LocalCredential(WebLocalCredential*);
    LocalCredential(const String& id, const String& name, const KURL& avatar, const String& password);
};

} // namespace blink

#endif // Credential_h
