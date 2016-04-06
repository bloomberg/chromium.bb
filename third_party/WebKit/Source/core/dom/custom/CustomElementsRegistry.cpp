// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementsRegistry.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/ElementRegistrationOptions.h"

namespace blink {

CustomElementsRegistry::CustomElementsRegistry()
{
}

void CustomElementsRegistry::define(ScriptState* scriptState,
    const AtomicString& name, const ScriptValue& constructor,
    const ElementRegistrationOptions& options, ExceptionState& exceptionState)
{
    // TODO(kojii): Element definition process not implemented yet.
    // http://w3c.github.io/webcomponents/spec/custom/#dfn-element-definition
}

DEFINE_TRACE(CustomElementsRegistry)
{
}

} // namespace blink
