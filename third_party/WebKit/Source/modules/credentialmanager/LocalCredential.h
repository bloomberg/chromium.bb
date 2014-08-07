// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LocalCredential_h
#define LocalCredential_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "modules/credentialmanager/Credential.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalCredential FINAL : public Credential {
public:
    static LocalCredential* create(const String& id, const String& name, const String& avatarURL, const String& password);
    virtual ~LocalCredential();

    // LocalCredential.idl
    const String& password() const { return m_password; }

private:
    LocalCredential(const String& id, const String& name, const String& avatarURL, const String& password);

    String m_password;
};

} // namespace blink

#endif // Credential_h
