// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StylePropertyMap_h
#define StylePropertyMap_h

#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/UnionTypesCore.h"
#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/cssom/StyleValue.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT StylePropertyMap : public GarbageCollectedFinalized<StylePropertyMap>, public ScriptWrappable, public PairIterable<String, StyleValueOrStyleValueSequence> {
    WTF_MAKE_NONCOPYABLE(StylePropertyMap);
    DEFINE_WRAPPERTYPEINFO();
public:
    typedef HeapVector<Member<StyleValue>> StyleValueVector;

    virtual ~StylePropertyMap() { }

    // Accessors.
    StyleValue* get(const String& propertyName, ExceptionState&);
    StyleValueVector getAll(const String& propertyName, ExceptionState&);
    bool has(const String& propertyName, ExceptionState&);

    virtual StyleValueVector getAll(CSSPropertyID) = 0;
    virtual bool has(CSSPropertyID) = 0;

    virtual Vector<String> getProperties() = 0;

    // Modifiers.
    void set(const String& propertyName, StyleValueOrStyleValueSequenceOrString& item, ExceptionState&);
    void append(const String& propertyName, StyleValueOrStyleValueSequenceOrString& item, ExceptionState&);
    void remove(const String& propertyName, ExceptionState&);

    virtual void set(CSSPropertyID, StyleValueOrStyleValueSequenceOrString& item, ExceptionState&) = 0;
    virtual void append(CSSPropertyID, StyleValueOrStyleValueSequenceOrString& item, ExceptionState&) = 0;
    virtual void remove(CSSPropertyID, ExceptionState&) = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() { }

protected:
    StylePropertyMap() { }

    IterationSource* startIteration(ScriptState*, ExceptionState&) override { return nullptr; }
    StyleValueVector cssValueToStyleValueVector(CSSPropertyID, const CSSValue&);
};

} // namespace blink

#endif
