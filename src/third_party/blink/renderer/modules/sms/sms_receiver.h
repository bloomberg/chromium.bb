// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_RECEIVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_RECEIVER_H_

#include "base/macros.h"
#include "third_party/blink/public/mojom/sms/sms_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

class SMS;
class SMSReceiverOptions;

class SMSReceiver final : public EventTargetWithInlineData,
                          public ActiveScriptWrappable<SMSReceiver>,
                          public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(SMSReceiver);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SMSReceiver* Create(ScriptState*,
                             const SMSReceiverOptions*,
                             ExceptionState&);
  static SMSReceiver* Create(ScriptState*, ExceptionState&);

  SMSReceiver(ExecutionContext*, base::TimeDelta threshold);

  ~SMSReceiver() override;

  // EventTarget implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ActiveScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // SMSReceiver IDL interface.
  ScriptPromise start(ScriptState*);
  void stop();
  SMS* sms() const;
  DEFINE_ATTRIBUTE_EVENT_LISTENER(change, kChange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(timeout, kTimeout)

  void OnGetNextMessage(mojom::blink::SmsMessagePtr sms);

  void Trace(blink::Visitor*) override;

 private:
  Member<SMS> sms_;

  const base::TimeDelta timeout_;

  void StartMonitoring();

  mojom::blink::SmsManagerPtr service_;

  DISALLOW_COPY_AND_ASSIGN(SMSReceiver);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_RECEIVER_H_
