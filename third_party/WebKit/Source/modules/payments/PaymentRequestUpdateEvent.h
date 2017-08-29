// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentRequestUpdateEvent_h
#define PaymentRequestUpdateEvent_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/events/Event.h"
#include "modules/ModulesExport.h"
#include "modules/payments/PaymentRequestUpdateEventInit.h"
#include "modules/payments/PaymentUpdater.h"
#include "platform/Timer.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ScriptState;

class MODULES_EXPORT PaymentRequestUpdateEvent final : public Event,
                                                       public PaymentUpdater {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PaymentRequestUpdateEvent)

 public:
  ~PaymentRequestUpdateEvent() override;

  static PaymentRequestUpdateEvent* Create(
      ExecutionContext*,
      const AtomicString& type,
      const PaymentRequestUpdateEventInit& = PaymentRequestUpdateEventInit());

  void SetPaymentDetailsUpdater(PaymentUpdater*);

  void updateWith(ScriptState*, ScriptPromise, ExceptionState&);

  // PaymentUpdater:
  void OnUpdatePaymentDetails(const ScriptValue& details_script_value) override;
  void OnUpdatePaymentDetailsFailure(const String& error) override;

  DECLARE_VIRTUAL_TRACE();

  void OnUpdateEventTimeoutForTesting();

 private:
  PaymentRequestUpdateEvent(ExecutionContext*,
                            const AtomicString& type,
                            const PaymentRequestUpdateEventInit&);

  void OnUpdateEventTimeout(TimerBase*);

  Member<PaymentUpdater> updater_;
  bool wait_for_update_;
  TaskRunnerTimer<PaymentRequestUpdateEvent> abort_timer_;
};

}  // namespace blink

#endif  // PaymentRequestUpdateEvent_h
