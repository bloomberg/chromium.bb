// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/permissions/Permissions.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

Permissions::~Permissions()
{
}

DEFINE_TRACE(Permissions)
{
}

// static
ScriptPromise Permissions::query(ScriptState* scriptState, const AtomicString& permissionName)
{
    // FIXME: implement.
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "Feature not yet supported."));
}

} // namespace blink
