// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ActiveScriptWrappable.h"

#include "bindings/core/v8/ScriptWrappable.h"
#include "wtf/HashSet.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"

namespace blink {

namespace {

using ActiveScriptWrappableSetType = HashSet<const ActiveScriptWrappable*>;

ActiveScriptWrappableSetType& activeScriptWrappables()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<ActiveScriptWrappableSetType>, activeScriptWrappableSet, new ThreadSpecific<ActiveScriptWrappableSetType>());
    return *activeScriptWrappableSet;
}

} // namespace

ActiveScriptWrappable::ActiveScriptWrappable(ScriptWrappable* self)
    : m_scriptWrappable(self)
{
    activeScriptWrappables().add(this);
}

ActiveScriptWrappable::~ActiveScriptWrappable()
{
    activeScriptWrappables().remove(this);
}

ScriptWrappable* ActiveScriptWrappable::toScriptWrappable() const
{
    return m_scriptWrappable;
}

void ActiveScriptWrappable::traceActiveScriptWrappables(ScriptWrappableVisitor* visitor)
{
    for (auto activeWrappable : activeScriptWrappables()) {
        if (!activeWrappable->hasPendingActivity())
            continue;

        visitor->traceWrappers(activeWrappable->toScriptWrappable());
    }
}

} // namespace blink
