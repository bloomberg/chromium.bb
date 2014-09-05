// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/ScriptWrappable.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/V8DOMWrapper.h"

namespace blink {

#if COMPILER(MSVC)
__declspec(align(4))
#endif
struct SameSizeAsScriptWrappableBase { };

COMPILE_ASSERT(sizeof(ScriptWrappableBase) <= sizeof(SameSizeAsScriptWrappableBase), ScriptWrappableBase_should_stay_small);

struct SameSizeAsScriptWrappable : public ScriptWrappableBase {
    virtual ~SameSizeAsScriptWrappable() { }
    uintptr_t m_wrapperOrTypeInfo;
};

COMPILE_ASSERT(sizeof(ScriptWrappable) <= sizeof(SameSizeAsScriptWrappable), ScriptWrappable_should_stay_small);

v8::Handle<v8::Object> ScriptWrappable::wrap(v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(!DOMDataStore::containsWrapperNonTemplate(this, isolate));

    const WrapperTypeInfo* wrapperType = wrapperTypeInfo();

    v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, wrapperType, toScriptWrappableBase(), isolate);
    if (UNLIKELY(wrapper.IsEmpty()))
        return wrapper;

    wrapperType->installConditionallyEnabledProperties(wrapper, isolate);
    wrapperType->refObject(toScriptWrappableBase());
    V8DOMWrapper::associateObjectWithWrapperNonTemplate(this, wrapperType, wrapper, isolate);
    return wrapper;
}

} // namespace blink
