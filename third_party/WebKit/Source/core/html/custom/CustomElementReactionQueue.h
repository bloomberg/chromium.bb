// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementReactionQueue_h
#define CustomElementReactionQueue_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class CustomElementReaction;
class Element;

class CORE_EXPORT CustomElementReactionQueue final
    : public GarbageCollectedFinalized<CustomElementReactionQueue> {
  WTF_MAKE_NONCOPYABLE(CustomElementReactionQueue);

 public:
  CustomElementReactionQueue();
  ~CustomElementReactionQueue();

  void Trace(blink::Visitor*);

  void Add(CustomElementReaction*);
  void InvokeReactions(Element*);
  bool IsEmpty() { return reactions_.IsEmpty(); }
  void Clear();

 private:
  HeapVector<Member<CustomElementReaction>, 1> reactions_;
  size_t index_;
};

}  // namespace blink

#endif  // CustomElementReactionQueue_h
