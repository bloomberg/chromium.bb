// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_EVENT_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/payments/payment_handler_host.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/payments/payment_request_event_init.h"
#include "third_party/blink/renderer/modules/service_worker/extendable_event.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace WTF {
class AtomicString;
}

namespace blink {

class RespondWithObserver;
class ScriptPromiseResolver;
class ScriptState;
class ScriptValue;

class MODULES_EXPORT PaymentRequestEvent final : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PaymentRequestEvent* Create(const AtomicString& type,
                                     const PaymentRequestEventInit*);
  static PaymentRequestEvent* Create(
      const AtomicString& type,
      const PaymentRequestEventInit*,
      mojo::PendingRemote<payments::mojom::blink::PaymentHandlerHost> host,
      RespondWithObserver*,
      WaitUntilObserver*);

  PaymentRequestEvent(
      const AtomicString& type,
      const PaymentRequestEventInit*,
      mojo::PendingRemote<payments::mojom::blink::PaymentHandlerHost> host,
      RespondWithObserver*,
      WaitUntilObserver*);
  ~PaymentRequestEvent() override;

  const AtomicString& InterfaceName() const override;

  const String& topOrigin() const;
  const String& paymentRequestOrigin() const;
  const String& paymentRequestId() const;
  const HeapVector<Member<PaymentMethodData>>& methodData() const;
  const ScriptValue total(ScriptState*) const;
  const HeapVector<Member<PaymentDetailsModifier>>& modifiers() const;
  const String& instrumentKey() const;

  ScriptPromise openWindow(ScriptState*, const String& url);
  ScriptPromise changePaymentMethod(ScriptState*,
                                    const String& method_name,
                                    ExceptionState& exception_state);
  ScriptPromise changePaymentMethod(ScriptState*,
                                    const String& method_name,
                                    const ScriptValue& method_details,
                                    ExceptionState& exception_state);
  void respondWith(ScriptState*, ScriptPromise, ExceptionState&);

  void Trace(blink::Visitor*) override;

 private:
  void OnChangePaymentMethodResponse(
      payments::mojom::blink::PaymentMethodChangeResponsePtr);
  void OnHostConnectionError();

  String top_origin_;
  String payment_request_origin_;
  String payment_request_id_;
  HeapVector<Member<PaymentMethodData>> method_data_;
  Member<PaymentCurrencyAmount> total_;
  HeapVector<Member<PaymentDetailsModifier>> modifiers_;
  String instrument_key_;

  Member<ScriptPromiseResolver> change_payment_method_resolver_;
  Member<RespondWithObserver> observer_;
  mojo::Remote<payments::mojom::blink::PaymentHandlerHost>
      payment_handler_host_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestEvent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_EVENT_H_
