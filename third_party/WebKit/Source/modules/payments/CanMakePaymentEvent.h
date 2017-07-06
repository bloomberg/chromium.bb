// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanMakePaymentEvent_h
#define CanMakePaymentEvent_h

#include "bindings/core/v8/ScriptValue.h"
#include "modules/EventModules.h"
#include "modules/payments/CanMakePaymentEventInit.h"
#include "modules/payments/PaymentDetailsModifier.h"
#include "modules/payments/PaymentMethodData.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "platform/heap/Handle.h"

namespace WTF {
class AtomicString;
}

namespace blink {

class RespondWithObserver;
class ScriptState;

class MODULES_EXPORT CanMakePaymentEvent final : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(CanMakePaymentEvent);

 public:
  static CanMakePaymentEvent* Create(const AtomicString& type,
                                     const CanMakePaymentEventInit&);
  static CanMakePaymentEvent* Create(const AtomicString& type,
                                     const CanMakePaymentEventInit&,
                                     RespondWithObserver*,
                                     WaitUntilObserver*);
  ~CanMakePaymentEvent() override;

  const AtomicString& InterfaceName() const override;

  const String& topLevelOrigin() const;
  const String& paymentRequestOrigin() const;
  const HeapVector<PaymentMethodData>& methodData() const;
  const HeapVector<PaymentDetailsModifier>& modifiers() const;

  void respondWith(ScriptState*, ScriptPromise, ExceptionState&);

  DECLARE_VIRTUAL_TRACE();

 private:
  CanMakePaymentEvent(const AtomicString& type,
                      const CanMakePaymentEventInit&,
                      RespondWithObserver*,
                      WaitUntilObserver*);

  String top_level_origin_;
  String payment_request_origin_;
  HeapVector<PaymentMethodData> method_data_;
  HeapVector<PaymentDetailsModifier> modifiers_;

  Member<RespondWithObserver> observer_;
};

}  // namespace blink

#endif  // CanMakePaymentEvent_h
