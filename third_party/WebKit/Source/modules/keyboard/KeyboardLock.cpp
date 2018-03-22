// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/keyboard/KeyboardLock.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/frame/LocalFrame.h"
#include "platform/bindings/V8Binding.h"
#include "platform/heap/Persistent.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Functional.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

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
        state, V8String(state->GetIsolate(),
                        "Last lock() call has not finished yet."));
  }

  if (!EnsureServiceConnected()) {
    return ScriptPromise::Reject(
        state, V8String(state->GetIsolate(), "Current frame is detached."));
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
  // TODO(joedow): Reject with a DOMException once it has been defined in the
  // spec.
  if (result == mojom::KeyboardLockRequestResult::SUCCESS) {
    request_keylock_resolver_->Resolve();
  } else {
    request_keylock_resolver_->Reject();
  }
  request_keylock_resolver_ = nullptr;
}

void KeyboardLock::Trace(blink::Visitor* visitor) {
  visitor->Trace(request_keylock_resolver_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
