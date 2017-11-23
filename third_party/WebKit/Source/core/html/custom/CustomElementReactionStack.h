// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementReactionStack_h
#define CustomElementReactionStack_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class CustomElementReaction;
class CustomElementReactionQueue;
class Element;

// https://html.spec.whatwg.org/multipage/scripting.html#custom-element-reactions
class CORE_EXPORT CustomElementReactionStack final
    : public GarbageCollected<CustomElementReactionStack>,
      public TraceWrapperBase {
 public:
  CustomElementReactionStack();

  void Trace(blink::Visitor*);
  virtual void TraceWrappers(const ScriptWrappableVisitor*) const;

  void Push();
  void PopInvokingReactions();
  void EnqueueToCurrentQueue(Element*, CustomElementReaction*);
  void EnqueueToBackupQueue(Element*, CustomElementReaction*);
  void ClearQueue(Element*);

  static CustomElementReactionStack& Current();

 private:
  friend class CustomElementReactionStackTestSupport;

  using ElementReactionQueueMap =
      HeapHashMap<TraceWrapperMember<Element>,
                  Member<CustomElementReactionQueue>>;
  ElementReactionQueueMap map_;

  using ElementQueue = HeapVector<Member<Element>, 1>;
  HeapVector<Member<ElementQueue>> stack_;
  Member<ElementQueue> backup_queue_;

  void InvokeBackupQueue();
  void InvokeReactions(ElementQueue&);
  void Enqueue(Member<ElementQueue>&, Element*, CustomElementReaction*);

  DISALLOW_COPY_AND_ASSIGN(CustomElementReactionStack);
};

class CORE_EXPORT CustomElementReactionStackTestSupport final {
 private:
  friend class ResetCustomElementReactionStackForTest;

  CustomElementReactionStackTestSupport() = delete;
  static CustomElementReactionStack* SetCurrentForTest(
      CustomElementReactionStack*);
};

}  // namespace blink

#endif  // CustomElementReactionStack_h
