// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/modules/sms/sms_receiver.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/sms/sms_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/sms/sms.h"
#include "third_party/blink/renderer/modules/sms/sms_receiver_options.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {

const uint32_t kDefaultTimeoutSeconds = 60;

}  // namespace

SMSReceiver* SMSReceiver::Create(ScriptState* script_state,
                                 const SMSReceiverOptions* options,
                                 ExceptionState& exception_state) {
  int32_t timeout_seconds =
      options->hasTimeout() ? options->timeout() : kDefaultTimeoutSeconds;

  if (timeout_seconds <= 0) {
    exception_state.ThrowTypeError("Invalid timeout");
    return nullptr;
  }

  base::TimeDelta timeout = base::TimeDelta::FromSeconds(timeout_seconds);

  return MakeGarbageCollected<SMSReceiver>(ExecutionContext::From(script_state),
                                           timeout);
}

// static
SMSReceiver* SMSReceiver::Create(ScriptState* script_state,
                                 ExceptionState& exception_state) {
  return Create(script_state, SMSReceiverOptions::Create(), exception_state);
}

SMSReceiver::SMSReceiver(ExecutionContext* context, base::TimeDelta timeout)
    : ContextClient(context), timeout_(timeout) {}

SMSReceiver::~SMSReceiver() = default;

const AtomicString& SMSReceiver::InterfaceName() const {
  return event_target_names::kSMSReceiver;
}

ExecutionContext* SMSReceiver::GetExecutionContext() const {
  return ContextClient::GetExecutionContext();
}

bool SMSReceiver::HasPendingActivity() const {
  // This object should be considered active as long as there are registered
  // event listeners.
  return GetExecutionContext() && HasEventListeners();
}

ScriptPromise SMSReceiver::start(ScriptState* script_state) {
  // Validate options.
  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  LocalFrame* frame = GetFrame();
  if (!frame->IsMainFrame()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kNotAllowedError,
                          "Must be in top-level browsing context."));
  }

  StartMonitoring();

  return ScriptPromise::CastUndefined(script_state);
}

void SMSReceiver::StartMonitoring() {
  // See https://bit.ly/2S0zRAS for task types.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kInternalIPC);

  if (!service_) {
    GetExecutionContext()->GetInterfaceProvider()->GetInterface(
        mojo::MakeRequest(&service_, task_runner));
  }

  service_->GetNextMessage(timeout_, WTF::Bind(&SMSReceiver::OnGetNextMessage,
                                               WrapPersistent(this)));
}

void SMSReceiver::OnGetNextMessage(mojom::blink::SmsMessagePtr sms) {
  if (sms->status == mojom::blink::SmsStatus::kTimeout) {
    DispatchEvent(*Event::Create(event_type_names::kTimeout));
    return;
  }
  sms_ = MakeGarbageCollected<blink::SMS>(std::move(sms->content));
  DispatchEvent(*Event::Create(event_type_names::kChange));
}

SMS* SMSReceiver::sms() const {
  return sms_;
}

void SMSReceiver::Trace(Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ContextClient::Trace(visitor);
  visitor->Trace(sms_);
}

}  // namespace blink
