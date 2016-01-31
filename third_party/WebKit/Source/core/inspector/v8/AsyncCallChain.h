// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AsyncCallChain_h
#define AsyncCallChain_h

#include "wtf/Deque.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace blink {

class AsyncCallStack final : public RefCounted<AsyncCallStack> {
public:
    AsyncCallStack(const String&, v8::Local<v8::Object>);
    ~AsyncCallStack();

    String description() const { return m_description; }
    v8::Local<v8::Object> callFrames(v8::Isolate* isolate) const { return v8::Local<v8::Object>::New(isolate, m_callFrames); }
private:
    String m_description;
    v8::Global<v8::Object> m_callFrames;
};

using AsyncCallStackVector = Deque<RefPtr<AsyncCallStack>, 4>;

class AsyncCallChain final : public RefCounted<AsyncCallChain> {
public:
    static PassRefPtr<AsyncCallChain> create(v8::Local<v8::Context>, PassRefPtr<AsyncCallStack>, AsyncCallChain* prevChain, unsigned asyncCallChainMaxLength);
    ~AsyncCallChain();
    const AsyncCallStackVector& callStacks() const { return m_callStacks; }
    v8::Local<v8::Context> creationContext(v8::Isolate* isolate) const { return m_creationContext.Get(isolate); }

private:
    AsyncCallChain(v8::Local<v8::Context>, PassRefPtr<AsyncCallStack>, AsyncCallChain* prevChain, unsigned asyncCallChainMaxLength);

    AsyncCallStackVector m_callStacks;
    v8::Global<v8::Context> m_creationContext;
};

} // namespace blink


#endif // !defined(AsyncCallChain_h)
