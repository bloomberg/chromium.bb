// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ActiveScriptWrappable.h"

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/ScriptWrappableVisitor.h"
#include "wtf/HashSet.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"

namespace blink {

namespace {

using ActiveScriptWrappableSetType = PersistentHeapHashSet<WeakMember<const ActiveScriptWrappable>>;

ActiveScriptWrappableSetType& activeScriptWrappables()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<ActiveScriptWrappableSetType>, activeScriptWrappableSet, new ThreadSpecific<ActiveScriptWrappableSetType>());
    if (!activeScriptWrappableSet.isSet())
        activeScriptWrappableSet->registerAsStaticReference();
    return *activeScriptWrappableSet;
}

} // namespace

ActiveScriptWrappable::ActiveScriptWrappable(ScriptWrappable* self)
    : m_scriptWrappable(self)
{
    activeScriptWrappables().add(this);
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

        ScriptWrappable* wrappable = activeWrappable->toScriptWrappable();
        wrappable->wrapperTypeInfo()->traceWrappers(visitor, wrappable);
    }
}

} // namespace blink
