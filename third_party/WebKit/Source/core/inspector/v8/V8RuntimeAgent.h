// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8RuntimeAgent_h
#define V8RuntimeAgent_h

#include "core/CoreExport.h"
#include "core/InspectorBackendDispatcher.h"
#include "core/inspector/v8/V8Debugger.h"

#include <v8.h>

namespace blink {

class InjectedScriptManager;

class CORE_EXPORT V8RuntimeAgent : public InspectorBackendDispatcher::RuntimeCommandHandler, public V8Debugger::Agent<InspectorFrontend::Runtime> {
public:
    static PassOwnPtr<V8RuntimeAgent> create(InjectedScriptManager*, V8Debugger*);
    virtual ~V8RuntimeAgent() { }

    // Embedder callbacks.
    virtual void reportExecutionContextCreated(v8::Local<v8::Context>, const String& type, const String& origin, const String& humanReadableName, const String& frameId) = 0;
    virtual void reportExecutionContextDestroyed(v8::Local<v8::Context>) = 0;
};

} // namespace blink

#endif // V8RuntimeAgent_h
