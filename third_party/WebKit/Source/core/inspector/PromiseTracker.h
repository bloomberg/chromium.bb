// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PromiseTracker_h
#define PromiseTracker_h

#include "core/InspectorFrontend.h"
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

class PromiseTracker final : public NoBaseWillBeGarbageCollected<PromiseTracker> {
    WTF_MAKE_NONCOPYABLE(PromiseTracker);
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(PromiseTracker);
public:
    class Listener : public WillBeGarbageCollectedMixin {
    public:
        virtual ~Listener() { }
        virtual void didUpdatePromise(InspectorFrontend::Debugger::EventType::Enum, PassRefPtr<TypeBuilder::Debugger::PromiseDetails>) = 0;
    };
    Listener* listener() const { return m_listener; }

    static PassOwnPtrWillBeRawPtr<PromiseTracker> create(Listener* listener)
    {
        return adoptPtrWillBeNoop(new PromiseTracker(listener));
    }

    bool isEnabled() const { return m_isEnabled; }
    void setEnabled(bool enabled, bool captureStacks);

    void clear();

    void didReceiveV8PromiseEvent(ScriptState*, v8::Local<v8::Object> promise, v8::Local<v8::Value> parentPromise, int status);

    PassRefPtr<TypeBuilder::Array<TypeBuilder::Debugger::PromiseDetails> > promises();
    ScriptValue promiseById(int promiseId) const;

    class PromiseData;

    typedef WillBeHeapVector<RefPtrWillBeMember<PromiseData> > PromiseDataVector;
    typedef WillBeHeapHashMap<int, PromiseDataVector> PromiseDataMap;
    typedef WillBeHeapHashMap<int, RefPtrWillBeMember<PromiseData> > PromiseIdToDataMap;

    DECLARE_TRACE();

    PromiseDataMap& promiseDataMap() { return m_promiseDataMap; }
    PromiseIdToDataMap& promiseIdToDataMap() { return m_promiseIdToDataMap; }

private:
    explicit PromiseTracker(Listener*);

    int circularSequentialId();
    PassRefPtrWillBeRawPtr<PromiseData> createPromiseDataIfNeeded(ScriptState*, v8::Local<v8::Object> promise);

    int m_circularSequentialId;
    PromiseDataMap m_promiseDataMap;
    bool m_isEnabled;
    bool m_captureStacks;
    PromiseIdToDataMap m_promiseIdToDataMap;
    RawPtrWillBeMember<Listener> m_listener;
};

} // namespace blink

#endif // !defined(PromiseTracker_h)
