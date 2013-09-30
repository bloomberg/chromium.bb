/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CSSStyleDeclaration_h
#define CSSStyleDeclaration_h

#include "CSSPropertyNames.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/css/CSSVariablesIterator.h"
#include "core/css/CSSVariablesMap.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace WebCore {

class CSSProperty;
class CSSRule;
class CSSStyleSheet;
class CSSValue;
class ExceptionState;
class MutableStylePropertySet;
class VariablesIterator;

class CSSStyleDeclaration : public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(CSSStyleDeclaration); WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~CSSStyleDeclaration();

    virtual void ref() = 0;
    virtual void deref() = 0;

    virtual CSSRule* parentRule() const = 0;
    virtual String cssText() const = 0;
    virtual void setCssText(const String&, ExceptionState&) = 0;
    virtual unsigned length() const = 0;
    virtual String item(unsigned index) const = 0;
    virtual PassRefPtr<CSSValue> getPropertyCSSValue(const String& propertyName) = 0;
    virtual String getPropertyValue(const String& propertyName) = 0;
    virtual String getPropertyPriority(const String& propertyName) = 0;
    virtual String getPropertyShorthand(const String& propertyName) = 0;
    virtual bool isPropertyImplicit(const String& propertyName) = 0;
    virtual void setProperty(const String& propertyName, const String& value, const String& priority, ExceptionState&) = 0;
    virtual String removeProperty(const String& propertyName, ExceptionState&) = 0;

    PassRefPtr<CSSVariablesMap> var();
    virtual unsigned variableCount() const = 0;
    virtual String variableValue(const AtomicString& name) const = 0;
    virtual bool setVariableValue(const AtomicString& name, const String& value, ExceptionState&) = 0; // Return value indicates whether variable was added.
    virtual bool removeVariable(const AtomicString& name) = 0;
    virtual bool clearVariables(ExceptionState&) = 0;
    virtual PassRefPtr<CSSVariablesIterator> variablesIterator() const = 0;

    // CSSPropertyID versions of the CSSOM functions to support bindings and editing.
    // Use the non-virtual methods in the concrete subclasses when possible.
    // The CSSValue returned by this function should not be exposed to the web as it may be used by multiple documents at the same time.
    virtual PassRefPtr<CSSValue> getPropertyCSSValueInternal(CSSPropertyID) = 0;
    virtual String getPropertyValueInternal(CSSPropertyID) = 0;
    virtual void setPropertyInternal(CSSPropertyID, const String& value, bool important, ExceptionState&) = 0;

    virtual PassRefPtr<MutableStylePropertySet> copyProperties() const = 0;

    virtual bool cssPropertyMatches(CSSPropertyID, const CSSValue*) const = 0;
    virtual CSSStyleSheet* parentStyleSheet() const { return 0; }

protected:
    CSSStyleDeclaration()
    {
        ScriptWrappable::init(this);
    }
    RefPtr<CSSVariablesMap> m_variablesMap;
};

} // namespace WebCore

#endif // CSSStyleDeclaration_h
