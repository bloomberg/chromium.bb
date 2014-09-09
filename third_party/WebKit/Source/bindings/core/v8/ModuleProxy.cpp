// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/ModuleProxy.h"

#include "wtf/StdLibExtras.h"

namespace blink {

ModuleProxy& ModuleProxy::moduleProxy()
{
    DEFINE_STATIC_LOCAL(ModuleProxy, moduleProxy, ());
    return moduleProxy;
}

void ModuleProxy::didLeaveScriptContextForRecursionScope(ExecutionContext& executionContext)
{
    RELEASE_ASSERT(m_didLeaveScriptContextForRecursionScope);
    (*m_didLeaveScriptContextForRecursionScope)(executionContext);
}

void ModuleProxy::registerDidLeaveScriptContextForRecursionScope(void (*didLeaveScriptContext)(ExecutionContext&))
{
    m_didLeaveScriptContextForRecursionScope = didLeaveScriptContext;
}

} // namespace blink
