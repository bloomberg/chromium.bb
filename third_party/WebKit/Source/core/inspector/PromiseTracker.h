// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PromiseTracker_h
#define PromiseTracker_h

#include "core/InspectorTypeBuilder.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include <v8.h>

namespace blink {

class ScriptState;
class ScriptValue;

class PromiseTracker FINAL : public NoBaseWillBeGarbageCollected<PromiseTracker> {
    WTF_MAKE_NONCOPYABLE(PromiseTracker);
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(PromiseTracker);
public:
    static PassOwnPtrWillBeRawPtr<PromiseTracker> create()
    {
        return adoptPtrWillBeNoop(new PromiseTracker());
    }

    bool isEnabled() const { return m_isEnabled; }
    void setEnabled(bool);

    void clear();

    void didReceiveV8PromiseEvent(ScriptState*, v8::Handle<v8::Object> promise, v8::Handle<v8::Value> parentPromise, int status);

    PassRefPtr<TypeBuilder::Array<TypeBuilder::Debugger::PromiseDetails> > promises();
    ScriptValue promiseById(int promiseId) const;

    class PromiseData;

    typedef WillBeHeapVector<RefPtrWillBeMember<PromiseData> > PromiseDataVector;
    typedef WillBeHeapHashMap<int, PromiseDataVector> PromiseDataMap;

    void trace(Visitor*);

    PromiseDataMap& promiseDataMap() { return m_promiseDataMap; }

private:
    PromiseTracker();

    int circularSequentialId();
    PassRefPtrWillBeRawPtr<PromiseData> createPromiseDataIfNeeded(ScriptState*, v8::Handle<v8::Object> promise);

    int m_circularSequentialId;
    PromiseDataMap m_promiseDataMap;
    bool m_isEnabled;
};

} // namespace blink

#endif // !defined(PromiseTracker_h)
