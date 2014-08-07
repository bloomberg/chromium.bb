// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FederatedCredential_h
#define FederatedCredential_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "modules/credentialmanager/Credential.h"
#include "platform/heap/Handle.h"

namespace blink {

class FederatedCredential FINAL : public Credential {
public:
    static FederatedCredential* create(const String& id, const String& name, const String& avatarURL, const String& federation);
    virtual ~FederatedCredential();

    // FederatedCredential.idl
    const String& federation() const { return m_federation; }

private:
    FederatedCredential(const String& id, const String& name, const String& avatarURL, const String& federation);

    String m_federation;
};

} // namespace blink

#endif // FederatedCredential_h
