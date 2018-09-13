// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_RESPONSE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_RESPONSE_H_

#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/payments/payment_currency_amount.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class PaymentAddress;
class PaymentStateResolver;
class PaymentValidationErrors;
class ScriptState;

class MODULES_EXPORT PaymentResponse final
    : public EventTargetWithInlineData,
      public ContextLifecycleObserver,
      public ActiveScriptWrappable<PaymentResponse> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PaymentResponse);
  WTF_MAKE_NONCOPYABLE(PaymentResponse);

 public:
  PaymentResponse(ExecutionContext*,
                  payments::mojom::blink::PaymentResponsePtr,
                  PaymentAddress* shipping_address_,
                  PaymentStateResolver*,
                  const String& requestId);
  ~PaymentResponse() override;

  void Update(payments::mojom::blink::PaymentResponsePtr, PaymentAddress*);
  void UpdatePayerDetail(payments::mojom::blink::PayerDetailPtr);

  ScriptValue toJSONForBinding(ScriptState*) const;

  const String& requestId() const { return requestId_; }
  const String& methodName() const { return method_name_; }
  ScriptValue details(ScriptState*, ExceptionState&) const;
  PaymentAddress* shippingAddress() const { return shipping_address_.Get(); }
  const String& shippingOption() const { return shipping_option_; }
  const String& payerName() const { return payer_name_; }
  const String& payerEmail() const { return payer_email_; }
  const String& payerPhone() const { return payer_phone_; }

  ScriptPromise complete(ScriptState*, const String& result = "");
  ScriptPromise retry(ScriptState*, const PaymentValidationErrors&);

  bool HasPendingActivity() const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(payerdetailchange);

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  void Trace(blink::Visitor*) override;

 private:
  String requestId_;
  String method_name_;
  String stringified_details_;
  Member<PaymentAddress> shipping_address_;
  String shipping_option_;
  String payer_name_;
  String payer_email_;
  String payer_phone_;
  Member<PaymentStateResolver> payment_state_resolver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_RESPONSE_H_
