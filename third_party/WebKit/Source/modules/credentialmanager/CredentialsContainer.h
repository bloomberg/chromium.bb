// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CredentialsContainer_h
#define CredentialsContainer_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Credential;
class Dictionary;
class ScriptPromise;
class ScriptState;

class CredentialsContainer FINAL : public GarbageCollected<CredentialsContainer>, public ScriptWrappable {
public:
    static CredentialsContainer* create();

    // CredentialsContainer.h
    ScriptPromise request(ScriptState*, const Dictionary&);
    ScriptPromise notifySignedIn(ScriptState*, Credential* = 0);
    ScriptPromise notifyFailedSignIn(ScriptState*, Credential* = 0);
    ScriptPromise notifySignedOut(ScriptState*);

    virtual void trace(Visitor*) { }

private:
    CredentialsContainer();
};

} // namespace blink

#endif // CredentialsContainer_h
