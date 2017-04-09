// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentRequestUpdateEvent_h
#define PaymentRequestUpdateEvent_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/events/Event.h"
#include "modules/ModulesExport.h"
#include "modules/payments/PaymentRequestUpdateEventInit.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class PaymentUpdater;
class ScriptState;

class MODULES_EXPORT PaymentRequestUpdateEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~PaymentRequestUpdateEvent() override;

  static PaymentRequestUpdateEvent* Create(
      ExecutionContext*,
      const AtomicString& type,
      const PaymentRequestUpdateEventInit& = PaymentRequestUpdateEventInit());

  void SetPaymentDetailsUpdater(PaymentUpdater*);

  void updateWith(ScriptState*, ScriptPromise, ExceptionState&);

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
