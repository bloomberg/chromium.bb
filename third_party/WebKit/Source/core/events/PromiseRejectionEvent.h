// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PromiseRejectionEvent_h
#define PromiseRejectionEvent_h

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/TraceWrapperV8Reference.h"
#include "core/CoreExport.h"
#include "core/events/Event.h"
#include "core/events/PromiseRejectionEventInit.h"

namespace blink {

class CORE_EXPORT PromiseRejectionEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(PromiseRejectionEvent, Dispose);

 public:
  static PromiseRejectionEvent* Create(
      ScriptState* state,
      const AtomicString& type,
      const PromiseRejectionEventInit& initializer) {
    return new PromiseRejectionEvent(state, type, initializer);
  }

  ScriptValue reason(ScriptState*) const;
  ScriptPromise promise(ScriptState*) const;

  const AtomicString& InterfaceName() const override;

  // PromiseRejectionEvents are similar to ErrorEvents in that they can't be
  // observed across different worlds.
  bool CanBeDispatchedInWorld(const DOMWrapperWorld&) const override;

  DECLARE_VIRTUAL_TRACE();

  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  PromiseRejectionEvent(ScriptState*,
                        const AtomicString&,
                        const PromiseRejectionEventInit&);
  ~PromiseRejectionEvent() override;
  void Dispose();

  RefPtr<DOMWrapperWorld> world_;
  TraceWrapperV8Reference<v8::Value> promise_;
  TraceWrapperV8Reference<v8::Value> reason_;
};

}  // namespace blink

#endif  // PromiseRejectionEvent_h
