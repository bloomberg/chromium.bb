// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentInstruments_h
#define PaymentInstruments_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "components/payments/mojom/payment_app.mojom-blink.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class PaymentInstrument;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT PaymentInstruments final
    : public GarbageCollected<PaymentInstruments>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(PaymentInstruments);

 public:
  explicit PaymentInstruments(const payments::mojom::blink::PaymentManagerPtr&);

  ScriptPromise deleteInstrument(const String& instrument_key);
  ScriptPromise get(ScriptState*, const String& instrument_key);
  ScriptPromise keys();
  ScriptPromise has(const String& instrument_key);
  ScriptPromise set(ScriptState*,
                    const String& instrument_key,
                    const PaymentInstrument& details,
                    ExceptionState&);

  DECLARE_TRACE();

 private:
  void onSetPaymentInstrument(ScriptPromiseResolver*,
                              payments::mojom::blink::PaymentHandlerStatus);
  void onGetPaymentInstrument(ScriptPromiseResolver*,
                              payments::mojom::blink::PaymentInstrumentPtr,
                              payments::mojom::blink::PaymentHandlerStatus);

  const payments::mojom::blink::PaymentManagerPtr& manager_;
};

}  // namespace blink

#endif  // PaymentInstruments_h
