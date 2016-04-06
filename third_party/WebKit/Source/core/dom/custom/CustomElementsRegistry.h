// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementsRegistry_h
#define CustomElementsRegistry_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"

namespace blink {

class ElementRegistrationOptions;
class ExceptionState;
class ScriptState;
class ScriptValue;

class CORE_EXPORT CustomElementsRegistry final
    : public GarbageCollected<CustomElementsRegistry>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static CustomElementsRegistry* create()
    {
        return new CustomElementsRegistry();
    }

    void define(ScriptState*, const AtomicString& name,
        const ScriptValue& constructor, const ElementRegistrationOptions&,
        ExceptionState&);

    DECLARE_TRACE();

private:
    CustomElementsRegistry();
};

} // namespace blink

#endif // CustomElementsRegistry_h
