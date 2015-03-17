// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8TestingScope_h
#define V8TestingScope_h

#include "bindings/core/v8/ScriptState.h"

#include <v8.h>

namespace blink {

class V8TestingScope {
public:
    explicit V8TestingScope(v8::Isolate*);
    ScriptState* scriptState() const;
    v8::Isolate* isolate() const;
    ~V8TestingScope();

private:
    v8::HandleScope m_handleScope;
    v8::Context::Scope m_contextScope;
    RefPtr<ScriptState> m_scriptState;
};

} // namespace blink

#endif // V8TestingScope_h
