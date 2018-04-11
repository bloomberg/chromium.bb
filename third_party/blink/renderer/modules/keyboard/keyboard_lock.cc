// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/keyboard/keyboard_lock.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr char kFrameDetachedErrorMsg[] = "Current frame is detached.";

constexpr char kPreviousLockCallNotCompletedErrorMsg[] =
    "Last lock() call has not finished yet.";

constexpr char kNoValidKeyCodesErrorMsg[] =
    "No valid key codes passed into lock().";

constexpr char kChildFrameErrorMsg[] =
    "lock() must be called from a top-level browsing context.";

}  // namespace

KeyboardLock::KeyboardLock(ExecutionContext* context)
    : ContextLifecycleObserver(context) {}

KeyboardLock::~KeyboardLock() = default;

ScriptPromise KeyboardLock::lock(ScriptState* state,
                                 const Vector<String>& keycodes) {
  DCHECK(state);
  if (request_keylock_resolver_) {
    // TODO(joedow): Reject with a DOMException once it has been defined in the
    // spec. See https://github.com/w3c/keyboard-lock/issues/18.
    return ScriptPromise::Reject(
        state,
        V8String(state->GetIsolate(), kPreviousLockCallNotCompletedErrorMsg));
  }

  if (!EnsureServiceConnected()) {
    return ScriptPromise::Reject(
        state, V8String(state->GetIsolate(), kFrameDetachedErrorMsg));
  }

  request_keylock_resolver_ = ScriptPromiseResolver::Create(state);
  service_->RequestKeyboardLock(
      keycodes,
      WTF::Bind(&KeyboardLock::LockRequestFinished, WrapPersistent(this)));
  return request_keylock_resolver_->Promise();
}

void KeyboardLock::unlock() {
  if (!EnsureServiceConnected()) {
    // Current frame is detached.
    return;
  }

  service_->CancelKeyboardLock();
}

bool KeyboardLock::EnsureServiceConnected() {
  if (!service_) {
    LocalFrame* frame = GetFrame();
    if (!frame) {
      return false;
    }
    frame->GetInterfaceProvider().GetInterface(mojo::MakeRequest(&service_));
  }

  DCHECK(service_);
  return true;
}

void KeyboardLock::LockRequestFinished(
    mojom::KeyboardLockRequestResult result) {
  DCHECK(request_keylock_resolver_);
  switch (result) {
    case mojom::KeyboardLockRequestResult::kSuccess:
      request_keylock_resolver_->Resolve();
      break;
    case mojom::KeyboardLockRequestResult::kFrameDetachedError:
      request_keylock_resolver_->Reject(kFrameDetachedErrorMsg);
      break;
    case mojom::KeyboardLockRequestResult::kNoValidKeyCodesError:
      request_keylock_resolver_->Reject(kNoValidKeyCodesErrorMsg);
      break;
    case mojom::KeyboardLockRequestResult::kChildFrameError:
      request_keylock_resolver_->Reject(kChildFrameErrorMsg);
      break;
  }
  request_keylock_resolver_ = nullptr;
}

void KeyboardLock::Trace(blink::Visitor* visitor) {
  visitor->Trace(request_keylock_resolver_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
