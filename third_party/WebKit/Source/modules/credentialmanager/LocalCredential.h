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

class DOMFormData;
class WebLocalCredential;

class LocalCredential final : public Credential {
    DEFINE_WRAPPERTYPEINFO();
public:
    static LocalCredential* create(const String& id, const String& password, ExceptionState& exceptionState)
    {
        return create(id, password, emptyString(), emptyString(), exceptionState);
    }

    static LocalCredential* create(const String& id, const String& password, const String& name, ExceptionState& exceptionState)
    {
        return create(id, password, name, emptyString(), exceptionState);
    }

    static LocalCredential* create(const String& id, const String& password, const String& name, const String& avatar, ExceptionState&);
    static LocalCredential* create(WebLocalCredential*);

    // LocalCredential.idl
    const String& password() const;
    DOMFormData* formData() const { return m_formData.get(); };

    DECLARE_VIRTUAL_TRACE();

private:
    LocalCredential(WebLocalCredential*);
    LocalCredential(const String& id, const String& password, const String& name, const KURL& avatar);

    // FIXME: Reconsider use of GarbageCollectedFinalized once this can be a Member.
    RefPtrWillBeMember<DOMFormData> m_formData;
};

} // namespace blink

#endif // LocalCredential_h
