// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentInstruments_h
#define PaymentInstruments_h

#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/payments/payment_app.mojom-blink.h"

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

  ScriptPromise deleteInstrument(ScriptState*, const String& instrument_key);
  ScriptPromise get(ScriptState*, const String& instrument_key);
  ScriptPromise keys(ScriptState*);
  ScriptPromise has(ScriptState*, const String& instrument_key);
  ScriptPromise set(ScriptState*,
                    const String& instrument_key,
                    const PaymentInstrument& details,
                    ExceptionState&);
  ScriptPromise clear(ScriptState*);

  DECLARE_TRACE();

 private:
  void onDeletePaymentInstrument(ScriptPromiseResolver*,
                                 payments::mojom::blink::PaymentHandlerStatus);
  void onGetPaymentInstrument(ScriptPromiseResolver*,
                              payments::mojom::blink::PaymentInstrumentPtr,
                              payments::mojom::blink::PaymentHandlerStatus);
  void onKeysOfPaymentInstruments(ScriptPromiseResolver*,
                                  const Vector<String>&,
                                  payments::mojom::blink::PaymentHandlerStatus);
  void onHasPaymentInstrument(ScriptPromiseResolver*,
                              payments::mojom::blink::PaymentHandlerStatus);
  void onSetPaymentInstrument(ScriptPromiseResolver*,
                              payments::mojom::blink::PaymentHandlerStatus);
  void onClearPaymentInstruments(ScriptPromiseResolver*,
                                 payments::mojom::blink::PaymentHandlerStatus);

  const payments::mojom::blink::PaymentManagerPtr& manager_;
};

}  // namespace blink

#endif  // PaymentInstruments_h
