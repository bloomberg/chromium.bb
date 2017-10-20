/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CustomEvent_h
#define CustomEvent_h

#include "core/CoreExport.h"
#include "core/dom/events/CustomEventInit.h"
#include "core/dom/events/Event.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/TraceWrapperV8Reference.h"

namespace blink {

class CORE_EXPORT CustomEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~CustomEvent() override;

  static CustomEvent* Create() { return new CustomEvent; }

  static CustomEvent* Create(ScriptState* script_state,
                             const AtomicString& type,
                             const CustomEventInit& initializer) {
    return new CustomEvent(script_state, type, initializer);
  }

  void initCustomEvent(ScriptState*,
                       const AtomicString& type,
                       bool can_bubble,
                       bool cancelable,
                       const ScriptValue& detail);

  const AtomicString& InterfaceName() const override;

  ScriptValue detail(ScriptState*) const;

  virtual void Trace(blink::Visitor*);

  virtual void TraceWrappers(const ScriptWrappableVisitor*) const;

 private:
  CustomEvent();
  CustomEvent(ScriptState*,
              const AtomicString& type,
              const CustomEventInit& initializer);

  scoped_refptr<DOMWrapperWorld> world_;
  TraceWrapperV8Reference<v8::Value> detail_;
};

}  // namespace blink

#endif  // CustomEvent_h
