/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NewSVGAnimatedProperty_h
#define NewSVGAnimatedProperty_h

#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/svg/SVGParsingError.h"
#include "core/svg/properties/NewSVGPropertyTearOff.h"
#include "core/svg/properties/SVGPropertyInfo.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class SVGElement;

class NewSVGAnimatedPropertyBase : public RefCounted<NewSVGAnimatedPropertyBase> {
public:
    virtual ~NewSVGAnimatedPropertyBase();

    virtual NewSVGPropertyBase* currentValueBase() = 0;

    virtual void animationStarted() = 0;
    virtual void resetToBaseVal() = 0;
    virtual void animationEnded() = 0;
    virtual void animValWillChange() = 0;
    virtual void animValDidChange() = 0;

    void synchronizeAttribute();

    AnimatedPropertyType type() const
    {
        return m_type;
    }

    SVGElement* contextElement()
    {
        return m_contextElement;
    }

    const QualifiedName& attributeName() const
    {
        return m_attributeName;
    }

protected:
    NewSVGAnimatedPropertyBase(AnimatedPropertyType type, SVGElement* contextElement, const QualifiedName& attributeName)
        : m_type(type)
        , m_contextElement(contextElement)
        , m_attributeName(attributeName)
    {
    }

private:
    const AnimatedPropertyType m_type;

    // This reference is kept alive from V8 wrapper
    SVGElement* m_contextElement;

    const QualifiedName& m_attributeName;

    WTF_MAKE_NONCOPYABLE(NewSVGAnimatedPropertyBase);
};

template <typename Property>
class NewSVGAnimatedProperty : public NewSVGAnimatedPropertyBase {
public:
    typedef typename Property::TearOffType TearOffType;

    static PassRefPtr<NewSVGAnimatedProperty<Property> > create(SVGElement* contextElement, const QualifiedName& attributeName, PassRefPtr<Property> initialValue)
    {
        return adoptRef(new NewSVGAnimatedProperty<Property>(contextElement, attributeName, initialValue));
    }

    ~NewSVGAnimatedProperty()
    {
        ASSERT(!isAnimating());
    }

    bool isReadOnly() const
    {
        return m_isReadOnly;
    }

    void setReadOnly()
    {
        m_isReadOnly = true;
    }

    Property* baseValue()
    {
        return m_baseValue.get();
    }

    Property* currentValue()
    {
        return m_currentValue ? m_currentValue.get() : m_baseValue.get();
    }

    virtual NewSVGPropertyBase* currentValueBase()
    {
        return currentValue();
    }

    void setBaseValueAsString(const String& value, SVGParsingError& parseError)
    {
        TrackExceptionState es;

        m_baseValue->setValueAsString(value, es);

        if (es.hadException())
            parseError = ParsingAttributeFailedError;
    }

    bool isAnimating() const
    {
        // |m_currentValue| only exists while animation is active,
        // so this can be used to check if this property is being animated.
        return m_currentValue;
    }

    virtual void animationStarted()
    {
        ASSERT(!isAnimating());
        m_currentValue = m_baseValue->clone();

        if (m_animValTearOff)
            m_animValTearOff->setTarget(m_currentValue);
    }

    virtual void animationEnded()
    {
        ASSERT(isAnimating());
        m_currentValue.clear();

        if (m_animValTearOff)
            m_animValTearOff->setTarget(m_baseValue);
    }

    virtual void resetToBaseVal()
    {
        ASSERT(isAnimating());
        m_currentValue = m_baseValue->clone();

        if (m_animValTearOff)
            m_animValTearOff->setTarget(m_currentValue);
    }

    virtual void animValWillChange()
    {
        ASSERT(isAnimating());
    }

    virtual void animValDidChange()
    {
        ASSERT(isAnimating());
    }

    // SVGAnimated* DOM Spec implementations:

    // baseVal()/animVal() are only to be used from SVG DOM implementation.
    // Use currentValue() from C++ code.
    virtual TearOffType* baseVal()
    {
        if (!m_baseValTearOff)
            m_baseValTearOff = TearOffType::create(m_baseValue, contextElement(), PropertyIsNotAnimVal, attributeName());

        return m_baseValTearOff.get();
    }

    TearOffType* animVal()
    {
        if (!m_animValTearOff)
            m_animValTearOff = TearOffType::create(currentValue(), contextElement(), PropertyIsAnimVal, attributeName());

        return m_animValTearOff.get();
    }

protected:
    NewSVGAnimatedProperty(SVGElement* contextElement, const QualifiedName& attributeName, PassRefPtr<Property> initialValue)
        : NewSVGAnimatedPropertyBase(Property::classType(), contextElement, attributeName)
        , m_baseValue(initialValue)
    {
    }

private:
    bool m_isReadOnly;

    // When still (not animated):
    //     Both m_animValTearOff and m_baseValTearOff target m_baseValue.
    // When animated:
    //     m_animValTearOff targets m_currentValue.
    //     m_baseValTearOff targets m_baseValue.
    RefPtr<Property> m_baseValue;
    RefPtr<Property> m_currentValue;
    RefPtr<TearOffType> m_baseValTearOff;
    RefPtr<TearOffType> m_animValTearOff;
};

}

#endif // NewSVGAnimatedProperty_h
