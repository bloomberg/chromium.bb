// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIAccessResolver_h
#define MIDIAccessResolver_h

#include "bindings/v8/ScriptPromiseResolver.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class DOMError;
class DOMWrapperWorld;
class ExecutionContext;
class MIDIAccess;

class MIDIAccessResolver {
    WTF_MAKE_NONCOPYABLE(MIDIAccessResolver);
public:
    static PassOwnPtr<MIDIAccessResolver> create(PassRefPtr<ScriptPromiseResolver> resolver, v8::Isolate* isolate)
    {
        return adoptPtr(new MIDIAccessResolver(resolver, isolate));
    }
    ~MIDIAccessResolver();

    ScriptPromise promise() { return m_resolver->promise(); }

    void resolve(MIDIAccess*, ExecutionContext*);
    void reject(DOMError*, ExecutionContext*);

private:
    MIDIAccessResolver(PassRefPtr<ScriptPromiseResolver>, v8::Isolate*);

    RefPtr<ScriptPromiseResolver> m_resolver;
    RefPtr<DOMWrapperWorld> m_world;
};

} // namespace WebCore

#endif // #ifndef MIDIAccessResolver_h
