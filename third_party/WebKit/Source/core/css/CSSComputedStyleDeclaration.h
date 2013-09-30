/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef CSSComputedStyleDeclaration_h
#define CSSComputedStyleDeclaration_h

#include "core/css/CSSStyleDeclaration.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "wtf/HashMap.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class CSSPrimitiveValue;
class CSSValueList;
class Color;
class CustomFilterNumberParameter;
class CustomFilterParameter;
class ExceptionState;
class MutableStylePropertySet;
class Node;
class RenderObject;
class RenderStyle;
class SVGPaint;
class ShadowData;
class StylePropertySet;
class StylePropertyShorthand;

enum EUpdateLayout { DoNotUpdateLayout = false, UpdateLayout = true };

class CSSComputedStyleDeclaration : public CSSStyleDeclaration {
private:
    class ComputedCSSVariablesIterator : public CSSVariablesIterator {
    public:
        virtual ~ComputedCSSVariablesIterator() { }
        static PassRefPtr<ComputedCSSVariablesIterator> create(const HashMap<AtomicString, String>* variableMap) { return adoptRef(new ComputedCSSVariablesIterator(variableMap)); }
    private:
        explicit ComputedCSSVariablesIterator(const HashMap<AtomicString, String>* variableMap);
        virtual void advance() OVERRIDE;
        virtual bool atEnd() const OVERRIDE { return m_it == m_end; }
        virtual AtomicString name() const OVERRIDE;
        virtual String value() const OVERRIDE;
        bool m_active;
        typedef HashMap<AtomicString, String>::const_iterator VariablesMapIterator;
        VariablesMapIterator m_it;
        VariablesMapIterator m_end;
    };

public:
    static PassRefPtr<CSSComputedStyleDeclaration> create(PassRefPtr<Node> node, bool allowVisitedStyle = false, const String& pseudoElementName = String())
    {
        return adoptRef(new CSSComputedStyleDeclaration(node, allowVisitedStyle, pseudoElementName));
    }
    virtual ~CSSComputedStyleDeclaration();

    virtual void ref() OVERRIDE;
    virtual void deref() OVERRIDE;

    PassRefPtr<CSSValue> getPropertyCSSValue(CSSPropertyID) const;
    String getPropertyValue(CSSPropertyID) const;
    bool getPropertyPriority(CSSPropertyID) const;

    virtual PassRefPtr<MutableStylePropertySet> copyProperties() const OVERRIDE;

    PassRefPtr<CSSValue> getPropertyCSSValue(CSSPropertyID, EUpdateLayout) const;
    PassRefPtr<CSSValue> getFontSizeCSSValuePreferringKeyword() const;
    bool useFixedFontDefaultSize() const;
    PassRefPtr<CSSValue> getSVGPropertyCSSValue(CSSPropertyID, EUpdateLayout) const;

    PassRefPtr<MutableStylePropertySet> copyPropertiesInSet(const Vector<CSSPropertyID>&) const;

private:
    CSSComputedStyleDeclaration(PassRefPtr<Node>, bool allowVisitedStyle, const String&);

    // The styled node is either the node passed into getComputedStyle, or the
    // PseudoElement for :before and :after if they exist.
    // FIXME: This should be styledElement since in JS getComputedStyle only works
    // on Elements, but right now editing creates these for text nodes. We should fix
    // that.
    Node* styledNode() const;

    // CSSOM functions. Don't make these public.
    virtual CSSRule* parentRule() const;
    virtual unsigned length() const;
    virtual String item(unsigned index) const;
    PassRefPtr<RenderStyle> computeRenderStyle(CSSPropertyID) const;
    virtual PassRefPtr<CSSValue> getPropertyCSSValue(const String& propertyName);
    virtual String getPropertyValue(const String& propertyName);
    virtual String getPropertyPriority(const String& propertyName);
    virtual String getPropertyShorthand(const String& propertyName);
    virtual bool isPropertyImplicit(const String& propertyName);
    virtual void setProperty(const String& propertyName, const String& value, const String& priority, ExceptionState&);
    virtual String removeProperty(const String& propertyName, ExceptionState&);
    virtual String cssText() const;
    virtual void setCssText(const String&, ExceptionState&);
    virtual PassRefPtr<CSSValue> getPropertyCSSValueInternal(CSSPropertyID);
    virtual String getPropertyValueInternal(CSSPropertyID);
    virtual void setPropertyInternal(CSSPropertyID, const String& value, bool important, ExceptionState&);

    const HashMap<AtomicString, String>* variableMap() const;
    virtual unsigned variableCount() const OVERRIDE;
    virtual String variableValue(const AtomicString& name) const OVERRIDE;
    virtual bool setVariableValue(const AtomicString& name, const String& value, ExceptionState&) OVERRIDE;
    virtual bool removeVariable(const AtomicString& name) OVERRIDE;
    virtual bool clearVariables(ExceptionState&) OVERRIDE;
    virtual PassRefPtr<CSSVariablesIterator> variablesIterator() const OVERRIDE { return ComputedCSSVariablesIterator::create(variableMap()); }

    virtual bool cssPropertyMatches(CSSPropertyID, const CSSValue*) const OVERRIDE;

    PassRefPtr<CSSValue> valueForShadow(const ShadowData*, CSSPropertyID, const RenderStyle*) const;
    PassRefPtr<CSSPrimitiveValue> currentColorOrValidColor(RenderStyle*, const Color&) const;
    PassRefPtr<SVGPaint> adjustSVGPaintForCurrentColor(PassRefPtr<SVGPaint>, RenderStyle*) const;

    PassRefPtr<CSSValue> valueForFilter(const RenderObject*, const RenderStyle*) const;

    PassRefPtr<CSSValueList> valuesForShorthandProperty(const StylePropertyShorthand&) const;
    PassRefPtr<CSSValueList> valuesForSidesShorthand(const StylePropertyShorthand&) const;
    PassRefPtr<CSSValueList> valuesForBackgroundShorthand() const;
    PassRefPtr<CSSValueList> valuesForGridShorthand(const StylePropertyShorthand&) const;

    RefPtr<Node> m_node;
    PseudoId m_pseudoElementSpecifier;
    bool m_allowVisitedStyle;
    unsigned m_refCount;
};

} // namespace WebCore

#endif // CSSComputedStyleDeclaration_h
