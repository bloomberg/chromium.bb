// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentResponse_h
#define PaymentResponse_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "modules/ModulesExport.h"
#include "modules/payments/PaymentCurrencyAmount.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/payments/payment_request.mojom-blink.h"

namespace blink {

class ExceptionState;
class PaymentAddress;
class PaymentCompleter;
class ScriptState;

class MODULES_EXPORT PaymentResponse final
    : public GarbageCollectedFinalized<PaymentResponse>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(PaymentResponse);

 public:
  PaymentResponse(payments::mojom::blink::PaymentResponsePtr,
                  PaymentAddress* shipping_address_,
                  PaymentCompleter*,
                  const String& requestId);
  virtual ~PaymentResponse();

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

  void Trace(blink::Visitor*);

 private:
  String requestId_;
  String method_name_;
  String stringified_details_;
  Member<PaymentAddress> shipping_address_;
  String shipping_option_;
  String payer_name_;
  String payer_email_;
  String payer_phone_;
  Member<PaymentCompleter> payment_completer_;
};

}  // namespace blink

#endif  // PaymentResponse_h
