// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventTargetImpl_h
#define EventTargetImpl_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventTarget.h"
#include "platform/bindings/ScriptWrappable.h"

class ScriptState;

namespace blink {

// Constructible version of EventTarget. Calls to EventTarget
// constructor in JavaScript will return an instance of this class.
// We don't use EventTarget directly because EventTarget is an abstract
// class and and making it non-abstract is unfavorable  because it will
// increase the size of EventTarget and all of its subclasses with code
// that are mostly unnecessary for them, resulting in a performance
// decrease.
class CORE_EXPORT EventTargetImpl final : public EventTargetWithInlineData,
                                          public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(EventTargetImpl);

 public:
  static EventTargetImpl* Create(ScriptState*);

  ~EventTargetImpl() = default;

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  void Trace(blink::Visitor*) override;

 private:
  EventTargetImpl(ScriptState*);
};

}  // namespace blink

#endif  // EventTargetImpl_h
