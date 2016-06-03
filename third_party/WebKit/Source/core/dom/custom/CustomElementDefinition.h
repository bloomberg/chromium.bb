// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementDefinition_h
#define CustomElementDefinition_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/dom/custom/CustomElementDescriptor.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class ScriptState;
class Element;

class CORE_EXPORT CustomElementDefinition
    : public GarbageCollectedFinalized<CustomElementDefinition> {
    WTF_MAKE_NONCOPYABLE(CustomElementDefinition);
public:
    CustomElementDefinition(const CustomElementDescriptor&);
    virtual ~CustomElementDefinition();

    DECLARE_VIRTUAL_TRACE();

    const CustomElementDescriptor& descriptor() { return m_descriptor; }

    // TODO(yosin): To support Web Modules, introduce an abstract
    // class |CustomElementConstructor| to allow us to have JavaScript
    // and C++ constructors and ask the binding layer to convert
    // |CustomElementConstructor| to |ScriptValue|. Replace
    // |getConstructorForScript()| by |getConstructor() ->
    // CustomElementConstructor|.
    virtual ScriptValue getConstructorForScript() = 0;

    using ConstructionStack = HeapVector<Member<Element>, 1>;
    ConstructionStack& constructionStack()
    {
        return m_constructionStack;
    }

    void upgrade(Element*);

protected:
    // TODO(dominicc): Make this pure virtual when the script side is
    // implemented.
    virtual bool runConstructor(Element*);

private:
    const CustomElementDescriptor m_descriptor;
    ConstructionStack m_constructionStack;
};

} // namespace blink

#endif // CustomElementDefinition_h
