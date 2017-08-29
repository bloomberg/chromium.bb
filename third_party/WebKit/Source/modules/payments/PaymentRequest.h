// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentRequest_h
#define PaymentRequest_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventTarget.h"
#include "modules/ModulesExport.h"
#include "modules/payments/PaymentCompleter.h"
#include "modules/payments/PaymentMethodData.h"
#include "modules/payments/PaymentOptions.h"
#include "modules/payments/PaymentUpdater.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/Timer.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/payments/payment_request.mojom-blink.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class PaymentAddress;
class PaymentDetailsInit;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT PaymentRequest final
    : public EventTargetWithInlineData,
      public payments::mojom::blink::PaymentRequestClient,
      public PaymentCompleter,
      public PaymentUpdater,
      public ContextLifecycleObserver,
      public ActiveScriptWrappable<PaymentRequest> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PaymentRequest)
  WTF_MAKE_NONCOPYABLE(PaymentRequest);

 public:
  static PaymentRequest* Create(ExecutionContext*,
                                const HeapVector<PaymentMethodData>&,
                                const PaymentDetailsInit&,
                                ExceptionState&);
  static PaymentRequest* Create(ExecutionContext*,
                                const HeapVector<PaymentMethodData>&,
                                const PaymentDetailsInit&,
                                const PaymentOptions&,
                                ExceptionState&);

  virtual ~PaymentRequest();

  ScriptPromise show(ScriptState*);
  ScriptPromise abort(ScriptState*);

  const String& id() const { return id_; }
  PaymentAddress* getShippingAddress() const { return shipping_address_.Get(); }
  const String& shippingOption() const { return shipping_option_; }
  const String& shippingType() const { return shipping_type_; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(shippingaddresschange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(shippingoptionchange);

  ScriptPromise canMakePayment(ScriptState*);

  // ScriptWrappable:
  bool HasPendingActivity() const override;

  // EventTargetWithInlineData:
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // PaymentCompleter:
  ScriptPromise Complete(ScriptState*, PaymentComplete result) override;

  // PaymentUpdater:
  void OnUpdatePaymentDetails(const ScriptValue& details_script_value) override;
  void OnUpdatePaymentDetailsFailure(const String& error) override;

  DECLARE_TRACE();

  void OnCompleteTimeoutForTesting();

 private:
  PaymentRequest(ExecutionContext*,
                 const HeapVector<PaymentMethodData>&,
                 const PaymentDetailsInit&,
                 const PaymentOptions&,
                 ExceptionState&);

  // LifecycleObserver:
  void ContextDestroyed(ExecutionContext*) override;

  // payments::mojom::blink::PaymentRequestClient:
  void OnShippingAddressChange(
      payments::mojom::blink::PaymentAddressPtr) override;
  void OnShippingOptionChange(const String& shipping_option_id) override;
  void OnPaymentResponse(payments::mojom::blink::PaymentResponsePtr) override;
  void OnError(payments::mojom::blink::PaymentErrorReason) override;
  void OnComplete() override;
  void OnAbort(bool aborted_successfully) override;
  void OnCanMakePayment(
      payments::mojom::blink::CanMakePaymentQueryResult) override;
  void WarnNoFavicon() override;

  void OnCompleteTimeout(TimerBase*);

  // Clears the promise resolvers and closes the Mojo connection.
  void ClearResolversAndCloseMojoConnection();

  PaymentOptions options_;
  Member<PaymentAddress> shipping_address_;
  String id_;
  String shipping_option_;
  String shipping_type_;
  Member<ScriptPromiseResolver> show_resolver_;
  Member<ScriptPromiseResolver> complete_resolver_;
  Member<ScriptPromiseResolver> abort_resolver_;
  Member<ScriptPromiseResolver> can_make_payment_resolver_;
  payments::mojom::blink::PaymentRequestPtr payment_provider_;
  mojo::Binding<payments::mojom::blink::PaymentRequestClient> client_binding_;
  TaskRunnerTimer<PaymentRequest> complete_timer_;
};

}  // namespace blink

#endif  // PaymentRequest_h
