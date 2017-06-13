// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentRequestEvent_h
#define PaymentRequestEvent_h

#include "bindings/core/v8/ScriptValue.h"
#include "modules/EventModules.h"
#include "modules/payments/PaymentRequestEventInit.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "platform/heap/Handle.h"

namespace WTF {
class AtomicString;
}

namespace blink {

class RespondWithObserver;
class ScriptState;

class MODULES_EXPORT PaymentRequestEvent final : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(PaymentRequestEvent);

 public:
  static PaymentRequestEvent* Create(const AtomicString& type,
                                     const PaymentRequestEventInit&);
  static PaymentRequestEvent* Create(const AtomicString& type,
                                     const PaymentRequestEventInit&,
                                     RespondWithObserver*,
                                     WaitUntilObserver*);
  ~PaymentRequestEvent() override;

  const AtomicString& InterfaceName() const override;

  const String& topLevelOrigin() const;
  const String& paymentRequestOrigin() const;
  const String& paymentRequestId() const;
  const HeapVector<PaymentMethodData>& methodData() const;
  const ScriptValue total(ScriptState*) const;
  const HeapVector<PaymentDetailsModifier>& modifiers() const;
  const String& instrumentKey() const;

  ScriptPromise openWindow(ScriptState*, const String& url);
  void respondWith(ScriptState*, ScriptPromise, ExceptionState&);

  DECLARE_VIRTUAL_TRACE();

 private:
  PaymentRequestEvent(const AtomicString& type,
                      const PaymentRequestEventInit&,
                      RespondWithObserver*,
                      WaitUntilObserver*);

  String top_level_origin_;
  String payment_request_origin_;
  String payment_request_id_;
  HeapVector<PaymentMethodData> method_data_;
  PaymentItem total_;
  HeapVector<PaymentDetailsModifier> modifiers_;
  String instrument_key_;

  Member<RespondWithObserver> observer_;
};

}  // namespace blink

#endif  // PaymentRequestEvent_h
