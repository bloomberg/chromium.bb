// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/modules/v8/ModuleBindingsInitializer.h"

#include "bindings/core/v8/ModuleProxy.h"
#include "bindings/core/v8/V8Event.h"
#include "bindings/core/v8/V8EventTarget.h"
#include "modules/EventModulesHeaders.h"
#include "modules/EventModulesInterfaces.h"
#include "modules/EventTargetModulesHeaders.h"
#include "modules/EventTargetModulesInterfaces.h"
#include "modules/indexeddb/IDBPendingTransactionMonitor.h"

namespace blink {

#define TRY_TO_WRAP_WITH_INTERFACE(interfaceName) \
    if (EventNames::interfaceName == desiredInterface) \
        return wrap(static_cast<interfaceName*>(event), creationContext, isolate);

static v8::Handle<v8::Object> wrapForModuleEvent(Event* event, v8::Handle<v8::Object> creationContext, v8::Isolate *isolate)
{
    ASSERT(event);

    String desiredInterface = event->interfaceName();
    EVENT_MODULES_INTERFACES_FOR_EACH(TRY_TO_WRAP_WITH_INTERFACE);

    // Wrapping for core event types should have been tried before this
    // function was called, so |event| should have been a module event type.
    // If this ASSERT is hit, the event type was missing from both
    // enumerations.
    ASSERT_NOT_REACHED();
    return v8::Handle<v8::Object>();
}

#undef TRY_TO_WRAP_WITH_INTERFACE

#define TRY_TO_TOV8_WITH_INTERFACE(interfaceName) \
    if (EventTargetNames::interfaceName == desiredInterface) \
        return toV8(static_cast<interfaceName*>(impl), creationContext, isolate);

static v8::Handle<v8::Value> toV8ForModuleEventTarget(EventTarget* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    AtomicString desiredInterface = impl->interfaceName();
    EVENT_TARGET_MODULES_INTERFACES_FOR_EACH(TRY_TO_TOV8_WITH_INTERFACE)

    ASSERT_NOT_REACHED();
    return v8Undefined();
}

#undef TRY_TO_TOV8_WITH_INTERFACE

static void didLeaveScriptContextForModule(ExecutionContext& executionContext)
{
    // Indexed DB requires that transactions are created with an internal |active| flag
    // set to true, but the flag becomes false when control returns to the event loop.
    IDBPendingTransactionMonitor::from(executionContext).deactivateNewTransactions();
}

void ModuleBindingsInitializer::init()
{
    ModuleProxy::moduleProxy().registerWrapForEvent(wrapForModuleEvent);
    ModuleProxy::moduleProxy().registerToV8ForEventTarget(toV8ForModuleEventTarget);
    ModuleProxy::moduleProxy().registerDidLeaveScriptContextForRecursionScope(didLeaveScriptContextForModule);
}

} // namespace blink
