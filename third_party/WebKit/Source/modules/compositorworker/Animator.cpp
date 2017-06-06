// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/Animator.h"

#include "modules/compositorworker/AnimatorDefinition.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

Animator::Animator(v8::Isolate* isolate,
                   AnimatorDefinition* definition,
                   v8::Local<v8::Object> instance)
    : definition_(this, definition), instance_(isolate, this, instance) {}

Animator::~Animator() {}

DEFINE_TRACE(Animator) {
  visitor->Trace(definition_);
}

DEFINE_TRACE_WRAPPERS(Animator) {
  visitor->TraceWrappers(definition_);
  visitor->TraceWrappers(instance_.Cast<v8::Value>());
}

}  // namespace blink
