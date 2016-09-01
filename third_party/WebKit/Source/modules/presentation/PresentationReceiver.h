// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationReceiver_h
#define PresentationReceiver_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/frame/DOMWindowProperty.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"

namespace blink {

class PresentationConnectionList;
class PresentationReceiver;

using ConnectionListProperty = ScriptPromiseProperty<Member<PresentationReceiver>, Member<PresentationConnectionList>, Member<DOMException>>;

// Implements the PresentationReceiver interface from the Presentation API from
// which websites can implement the receiving side of a presentation session.
class PresentationReceiver final
    : public GarbageCollected<PresentationReceiver>
    , public ScriptWrappable
    , public DOMWindowProperty {
    USING_GARBAGE_COLLECTED_MIXIN(PresentationReceiver);
    DEFINE_WRAPPERTYPEINFO();
public:
    explicit PresentationReceiver(LocalFrame*);
    ~PresentationReceiver() = default;

    ScriptPromise connectionList(ScriptState*);

    DECLARE_VIRTUAL_TRACE();

private:
    Member<ConnectionListProperty> m_connectionListProperty;
};

} // namespace blink

#endif // PresentationReceiver_h
