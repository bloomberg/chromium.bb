// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/modules/v8/ModuleBindingsInitializer.h"

#include "bindings/core/v8/ModuleProxy.h"
#include "core/dom/ExecutionContext.h"
#include "modules/indexeddb/IDBPendingTransactionMonitor.h"

namespace blink {

static void didLeaveScriptContextForModule(ExecutionContext& executionContext)
{
    // Indexed DB requires that transactions are created with an internal |active| flag
    // set to true, but the flag becomes false when control returns to the event loop.
    IDBPendingTransactionMonitor::from(executionContext).deactivateNewTransactions();
}

void ModuleBindingsInitializer::init()
{
    ModuleProxy::moduleProxy().registerDidLeaveScriptContextForRecursionScope(didLeaveScriptContextForModule);
}

} // namespace blink
