// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ComputedAccessibleNode.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/Element.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

ComputedAccessibleNode* ComputedAccessibleNode::Create(Element* element) {
  return new ComputedAccessibleNode(element);
}

ComputedAccessibleNode::ComputedAccessibleNode(Element* element)
    : element_(element) {
  DCHECK(RuntimeEnabledFeatures::AccessibilityObjectModelEnabled());
}

ComputedAccessibleNode::~ComputedAccessibleNode() {}

ScriptPromise ComputedAccessibleNode::ComputePromiseProperty(
    ScriptState* script_state) {
  if (!computed_property_) {
    computed_property_ =
        new ComputedPromiseProperty(ExecutionContext::From(script_state), this,
                                    ComputedPromiseProperty::kReady);
    computed_property_->Resolve(this);
  }

  return computed_property_->Promise(script_state->World());
}

const AtomicString& ComputedAccessibleNode::role() const {
  return element_->computedRole();
}

const String ComputedAccessibleNode::name() const {
  return element_->computedName();
}

void ComputedAccessibleNode::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(computed_property_);
  visitor->Trace(element_);
}

}  // namespace blink
