// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Credential_h
#define Credential_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "platform/heap/Handle.h"

namespace blink {

class Credential : public GarbageCollectedFinalized<Credential>, public ScriptWrappable {
public:
    static Credential* create(const String& id, const String& name, const String& avatarURL);
    virtual ~Credential();

    // Credential.idl
    const String& id() const { return m_id; }
    const String& name() const { return m_name; }
    const String& avatarURL() const { return m_avatarURL; }

    virtual void trace(Visitor*) { };

protected:
    Credential(const String& id, const String& name, const String& avatarURL);

private:
    String m_id;
    String m_name;
    String m_avatarURL;
};

} // namespace blink

#endif // Credential_h
