// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Credential_h
#define Credential_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "platform/credentialmanager/PlatformCredential.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"

namespace blink {

class WebCredential;
class ExceptionState;

class Credential : public GarbageCollected<Credential>, public ScriptWrappable {
public:
    static Credential* create(const String& id, const String& name, const KURL& avatar);
    static Credential* create(const String& id, const String& name, const String& avatar, ExceptionState&);

    // Credential.idl
    const String& id() const { return m_platformCredential->id(); }
    const String& name() const { return m_platformCredential->name(); }
    const KURL& avatarURL() const { return m_platformCredential->avatarURL(); }

    virtual void trace(Visitor*);

protected:
    explicit Credential(PlatformCredential*);
    Credential(const String& id, const String& name, const KURL& avatar);

    // Parses a string as a KURL. Throws an exception via |exceptionState| if an invalid URL is produced.
    static KURL parseStringAsURL(const String&, ExceptionState&);

    Member<PlatformCredential> m_platformCredential;
};

} // namespace blink

#endif // Credential_h
