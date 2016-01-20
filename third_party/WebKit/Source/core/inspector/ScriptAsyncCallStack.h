// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptAsyncCallStack_h
#define ScriptAsyncCallStack_h

#include "core/InspectorTypeBuilder.h"
#include "core/inspector/ScriptCallStack.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"

namespace blink {

class ScriptAsyncCallStack : public RefCounted<ScriptAsyncCallStack> {
public:
    static PassRefPtr<ScriptAsyncCallStack> create(const String&, PassRefPtr<ScriptCallStack>, PassRefPtr<ScriptAsyncCallStack>);

    ~ScriptAsyncCallStack();

    PassRefPtr<TypeBuilder::Console::AsyncStackTrace> buildInspectorObject() const;

private:
    ScriptAsyncCallStack(const String&, PassRefPtr<ScriptCallStack>, PassRefPtr<ScriptAsyncCallStack>);

    String m_description;
    RefPtr<ScriptCallStack> m_callStack;
    RefPtr<ScriptAsyncCallStack> m_asyncStackTrace;
};

} // namespace blink

#endif // ScriptAsyncCallStack_h
