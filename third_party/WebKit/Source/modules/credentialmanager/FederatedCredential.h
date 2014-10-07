// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FederatedCredential_h
#define FederatedCredential_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "modules/credentialmanager/Credential.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"

namespace blink {

class WebFederatedCredential;

class FederatedCredential FINAL : public Credential {
    DEFINE_WRAPPERTYPEINFO();
public:
    static FederatedCredential* create(WebFederatedCredential*);
    static FederatedCredential* create(const String& id, const String& name, const String& avatar, const String& federation, ExceptionState&);

    // FederatedCredential.idl
    const KURL& federation() const;

private:
    FederatedCredential(WebFederatedCredential*);
    FederatedCredential(const String& id, const String& name, const KURL& avatar, const KURL& federation);
};

} // namespace blink

#endif // FederatedCredential_h
