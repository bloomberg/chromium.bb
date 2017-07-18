// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AbortPaymentEvent_h
#define AbortPaymentEvent_h

#include "modules/EventModules.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"

namespace WTF {
class AtomicString;
}

namespace blink {

class ExtendableEventInit;
class RespondWithObserver;
class ScriptState;

class MODULES_EXPORT AbortPaymentEvent final : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(AbortPaymentEvent);

 public:
  static AbortPaymentEvent* Create(const AtomicString& type,
                                   const ExtendableEventInit&);
  static AbortPaymentEvent* Create(const AtomicString& type,
                                   const ExtendableEventInit&,
                                   RespondWithObserver*,
                                   WaitUntilObserver*);
  ~AbortPaymentEvent() override;

  const AtomicString& InterfaceName() const override;

  void respondWith(ScriptState*, ScriptPromise, ExceptionState&);

  DECLARE_VIRTUAL_TRACE();

 private:
  AbortPaymentEvent(const AtomicString& type,
                    const ExtendableEventInit&,
                    RespondWithObserver*,
                    WaitUntilObserver*);

  Member<RespondWithObserver> observer_;
};

}  // namespace blink

#endif  // AbortPaymentEvent_h
