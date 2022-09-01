// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/eyedropper/eye_dropper.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_color_selection_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_color_selection_result.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/base/ui_base_features.h"

namespace blink {

class EyeDropper::OpenAbortAlgorithm final : public AbortSignal::Algorithm {
 public:
  OpenAbortAlgorithm(EyeDropper* eyedropper, AbortSignal* signal)
      : eyedropper_(eyedropper), abortsignal_(signal) {}
  ~OpenAbortAlgorithm() override = default;

  void Run() override { eyedropper_->AbortCallback(abortsignal_); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(eyedropper_);
    visitor->Trace(abortsignal_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<EyeDropper> eyedropper_;
  Member<AbortSignal> abortsignal_;
};

constexpr char kNotAvailableMessage[] = "EyeDropper is not available.";

EyeDropper::EyeDropper(ExecutionContext* context)
    : eye_dropper_chooser_(context) {}

EyeDropper* EyeDropper::Create(ExecutionContext* context) {
  return MakeGarbageCollected<EyeDropper>(context);
}

ScriptPromise EyeDropper::open(ScriptState* script_state,
                               const ColorSelectionOptions* options,
                               ExceptionState& exception_state) {
  DCHECK(RuntimeEnabledFeatures::EyeDropperAPIEnabled());

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The object is no longer associated with a window.");
    return ScriptPromise();
  }

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!LocalFrame::HasTransientUserActivation(window->GetFrame())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "EyeDropper::open() requires user gesture.");
    return ScriptPromise();
  }

  if (!features::IsEyeDropperEnabled()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      kNotAvailableMessage);
    return ScriptPromise();
  }

  if (eye_dropper_chooser_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "EyeDropper is already open.");
    return ScriptPromise();
  }

  if (options->hasSignal()) {
    signal_ = options->signal();
    if (signal_->aborted()) {
      return ScriptPromise::Reject(script_state, signal_->reason(script_state));
    }
    signal_->AddAlgorithm(
        MakeGarbageCollected<OpenAbortAlgorithm>(this, signal_));
  }

  resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver_->Promise();

  auto* frame = window->GetFrame();
  frame->GetBrowserInterfaceBroker().GetInterface(
      eye_dropper_chooser_.BindNewPipeAndPassReceiver(
          frame->GetTaskRunner(TaskType::kUserInteraction)));
  eye_dropper_chooser_.set_disconnect_handler(
      WTF::Bind(&EyeDropper::EndChooser, WrapWeakPersistent(this)));
  eye_dropper_chooser_->Choose(resolver_->WrapCallbackInScriptScope(
      WTF::Bind(&EyeDropper::EyeDropperResponseHandler, WrapPersistent(this))));

  return promise;
}

void EyeDropper::AbortCallback(AbortSignal* signal) {
  // There is no way to remove abort signal callbacks, so we need to
  // perform null-check for `resolver_` to see if the promise has already
  // been resolved.
  // TODO(https://crbug.com/1296280): It should be possible to remove abort
  // callbacks. This object can be reused for multiple eyedropper operations,
  // and it might be possible for multiple abort signals to be mixed up.

  // There is no RemoveAlgorithm() method on AbortSignal so compare the signal
  // bound to this callback to the one last passed to open().
  if (signal_ != signal)
    return;

  if (resolver_) {
    ScriptState* script_state = resolver_->GetScriptState();
    if (IsInParallelAlgorithmRunnable(resolver_->GetExecutionContext(),
                                      script_state)) {
      ScriptState::Scope script_state_scope(script_state);
      resolver_->Reject(signal_->reason(script_state));
    }
  }

  eye_dropper_chooser_.reset();
  resolver_ = nullptr;
}

void EyeDropper::EyeDropperResponseHandler(ScriptPromiseResolver* resolver,
                                           bool success,
                                           uint32_t color) {
  eye_dropper_chooser_.reset();

  // The abort callback resets the Mojo remote if an abort is signalled,
  // so by receiving a reply, the eye dropper operation must *not* have
  // been aborted by the abort signal. Thus, the promise is not yet resolved,
  // so resolver_ must be non-null.
  DCHECK_EQ(resolver_, resolver);

  if (success) {
    ColorSelectionResult* result = ColorSelectionResult::Create();
    result->setSRGBHex(Color(color).Serialized());
    resolver->Resolve(result);
  } else {
    RejectPromiseHelper(DOMExceptionCode::kAbortError,
                        "The user canceled the selection.");
  }
}

void EyeDropper::EndChooser() {
  eye_dropper_chooser_.reset();

  if (!resolver_ ||
      !IsInParallelAlgorithmRunnable(resolver_->GetExecutionContext(),
                                     resolver_->GetScriptState()))
    return;

  ScriptState::Scope script_state_scope(resolver_->GetScriptState());

  RejectPromiseHelper(DOMExceptionCode::kOperationError, kNotAvailableMessage);
}

void EyeDropper::RejectPromiseHelper(DOMExceptionCode exception_code,
                                     const WTF::String& message) {
  resolver_->RejectWithDOMException(exception_code, message);
  resolver_ = nullptr;
}

void EyeDropper::Trace(Visitor* visitor) const {
  visitor->Trace(eye_dropper_chooser_);
  visitor->Trace(resolver_);
  visitor->Trace(signal_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
