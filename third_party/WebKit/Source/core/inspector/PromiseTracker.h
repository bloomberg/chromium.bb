// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PromiseTracker_h
#define PromiseTracker_h

#include "core/InspectorTypeBuilder.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include <v8.h>

namespace blink {

class ScriptState;

class PromiseTracker {
    WTF_MAKE_NONCOPYABLE(PromiseTracker);
public:
    PromiseTracker();
    ~PromiseTracker();

    bool isEnabled() const { return m_isEnabled; }
    void setEnabled(bool);

    void clear();

    void didReceiveV8PromiseEvent(ScriptState*, v8::Handle<v8::Object> promise, v8::Handle<v8::Value> parentPromise, int status);

    PassRefPtr<TypeBuilder::Array<TypeBuilder::Debugger::PromiseDetails> > promises();

    class PromiseData;

    typedef Vector<RefPtr<PromiseData> > PromiseDataVector;
    typedef HashMap<int, PromiseDataVector> PromiseDataMap;

private:
    int circularSequentialId();
    PassRefPtr<PromiseData> createPromiseDataIfNeeded(v8::Isolate*, v8::Handle<v8::Object> promise);

    bool m_isEnabled;
    int m_circularSequentialId;
    PromiseDataMap m_promiseDataMap;
};

} // namespace blink

#endif // !defined(PromiseTracker_h)
