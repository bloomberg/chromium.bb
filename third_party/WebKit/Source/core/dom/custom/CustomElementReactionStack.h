// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementReactionStack_h
#define CustomElementReactionStack_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CustomElementReaction;
class CustomElementReactionQueue;
class Element;

// https://html.spec.whatwg.org/multipage/scripting.html#custom-element-reactions
class CORE_EXPORT CustomElementReactionStack final
    : public GarbageCollectedFinalized<CustomElementReactionStack> {
    WTF_MAKE_NONCOPYABLE(CustomElementReactionStack);
public:
    CustomElementReactionStack();
    ~CustomElementReactionStack();

    DECLARE_TRACE();

    void push();
    void popInvokingReactions();
    void enqueue(Element*, CustomElementReaction*);

private:
    using ElementReactionQueueMap =
        HeapHashMap<Member<Element>, Member<CustomElementReactionQueue>>;
    ElementReactionQueueMap m_map;

    using ElementQueue = HeapVector<Member<Element>, 1>;
    HeapVector<Member<ElementQueue>> m_stack;
};

} // namespace blink

#endif // CustomElementReactionStack_h
