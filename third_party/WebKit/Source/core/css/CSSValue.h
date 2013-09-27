/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef CSSValue_h
#define CSSValue_h

#include "core/dom/ExceptionCode.h"
#include "weborigin/KURL.h"
#include "wtf/ListHashSet.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class ExceptionState;
class StyleSheetContents;

enum CssTextFormattingFlags { QuoteCSSStringIfNeeded, AlwaysQuoteCSSString };

// FIXME: The current CSSValue and subclasses should be turned into internal types (StyleValue).
// The few subtypes that are actually exposed in CSSOM can be seen in the cloneForCSSOM() function.
// They should be handled by separate wrapper classes.

// Please don't expose more CSSValue types to the web.
class CSSValue : public RefCounted<CSSValue> {
public:
    enum Type {
        CSS_INHERIT = 0,
        CSS_PRIMITIVE_VALUE = 1,
        CSS_VALUE_LIST = 2,
        CSS_CUSTOM = 3,
        CSS_INITIAL = 4

    };

    // Override RefCounted's deref() to ensure operator delete is called on
    // the appropriate subclass type.
    void deref()
    {
        if (derefBase())
            destroy();
    }

    Type cssValueType() const;

    String cssText() const;
    void setCssText(const String&, ExceptionState&) { } // FIXME: Not implemented.
    String serializeResolvingVariables(const HashMap<AtomicString, String>&) const;

    bool isPrimitiveValue() const { return m_classType == PrimitiveClass; }
    bool isValueList() const { return m_classType >= ValueListClass; }

    bool isBaseValueList() const { return m_classType == ValueListClass; }

    bool isAspectRatioValue() const { return m_classType == AspectRatioClass; }
    bool isBorderImageSliceValue() const { return m_classType == BorderImageSliceClass; }
    bool isCursorImageValue() const { return m_classType == CursorImageClass; }
    bool isFontFeatureValue() const { return m_classType == FontFeatureClass; }
    bool isFontValue() const { return m_classType == FontClass; }
    bool isFontFaceSrcValue() const { return m_classType == FontFaceSrcClass; }
    bool isFunctionValue() const { return m_classType == FunctionClass; }
    bool isImageGeneratorValue() const { return m_classType >= CanvasClass && m_classType <= RadialGradientClass; }
    bool isGradientValue() const { return m_classType >= LinearGradientClass && m_classType <= RadialGradientClass; }
    bool isImageSetValue() const { return m_classType == ImageSetClass; }
    bool isImageValue() const { return m_classType == ImageClass; }
    bool isImplicitInitialValue() const;
    bool isInheritedValue() const { return m_classType == InheritedClass; }
    bool isInitialValue() const { return m_classType == InitialClass; }
    bool isReflectValue() const { return m_classType == ReflectClass; }
    bool isShadowValue() const { return m_classType == ShadowClass; }
    bool isCubicBezierTimingFunctionValue() const { return m_classType == CubicBezierTimingFunctionClass; }
    bool isStepsTimingFunctionValue() const { return m_classType == StepsTimingFunctionClass; }
    bool isCSSTransformValue() const { return m_classType == CSSTransformClass; }
    bool isCSSLineBoxContainValue() const { return m_classType == LineBoxContainClass; }
    bool isCalcValue() const {return m_classType == CalculationClass; }
    bool isCSSFilterValue() const { return m_classType == CSSFilterClass; }
    bool isCSSArrayFunctionValue() const { return m_classType == CSSArrayFunctionValueClass; }
    bool isCSSMixFunctionValue() const { return m_classType == CSSMixFunctionValueClass; }
    bool isShaderValue() const { return m_classType == CSSShaderClass; }
    bool isVariableValue() const { return m_classType == VariableClass; }
    bool isGridTemplateValue() const { return m_classType == GridTemplateClass; }
    bool isSVGColor() const { return m_classType == SVGColorClass || m_classType == SVGPaintClass; }
    bool isSVGPaint() const { return m_classType == SVGPaintClass; }
    bool isSVGDocumentValue() const { return m_classType == CSSSVGDocumentClass; }

    bool isCSSOMSafe() const { return m_isCSSOMSafe; }
    bool isSubtypeExposedToCSSOM() const
    {
        return isPrimitiveValue() || isSVGColor() || isValueList();
    }

    PassRefPtr<CSSValue> cloneForCSSOM() const;

    void addSubresourceStyleURLs(ListHashSet<KURL>&, const StyleSheetContents*) const;

    bool hasFailedOrCanceledSubresources() const;

    bool equals(const CSSValue&) const;

protected:

    static const size_t ClassTypeBits = 6;
    enum ClassType {
        PrimitiveClass,

        // Image classes.
        ImageClass,
        CursorImageClass,

        // Image generator classes.
        CanvasClass,
        CrossfadeClass,
        LinearGradientClass,
        RadialGradientClass,

        // Timing function classes.
        CubicBezierTimingFunctionClass,
        StepsTimingFunctionClass,

        // Other class types.
        AspectRatioClass,
        BorderImageSliceClass,
        FontFeatureClass,
        FontClass,
        FontFaceSrcClass,
        FunctionClass,

        InheritedClass,
        InitialClass,

        ReflectClass,
        ShadowClass,
        UnicodeRangeClass,
        LineBoxContainClass,
        CalculationClass,
        CSSShaderClass,
        VariableClass,
        GridTemplateClass,

        // SVG classes.
        SVGColorClass,
        SVGPaintClass,
        CSSSVGDocumentClass,

        // List class types must appear after ValueListClass.
        ValueListClass,
        ImageSetClass,
        CSSFilterClass,
        CSSArrayFunctionValueClass,
        CSSMixFunctionValueClass,
        CSSTransformClass,
        // Do not append non-list class types here.
    };

    static const size_t ValueListSeparatorBits = 2;
    enum ValueListSeparator {
        SpaceSeparator,
        CommaSeparator,
        SlashSeparator
    };

    ClassType classType() const { return static_cast<ClassType>(m_classType); }

    explicit CSSValue(ClassType classType, bool isCSSOMSafe = false)
        : m_isCSSOMSafe(isCSSOMSafe)
        , m_isTextClone(false)
        , m_primitiveUnitType(0)
        , m_hasCachedCSSText(false)
        , m_isQuirkValue(false)
        , m_valueListSeparator(SpaceSeparator)
        , m_classType(classType)
    {
    }

    // NOTE: This class is non-virtual for memory and performance reasons.
    // Don't go making it virtual again unless you know exactly what you're doing!

    ~CSSValue() { }

private:
    void destroy();

protected:
    unsigned m_isCSSOMSafe : 1;
    unsigned m_isTextClone : 1;
    // The bits in this section are only used by specific subclasses but kept here
    // to maximize struct packing.

    // CSSPrimitiveValue bits:
    unsigned m_primitiveUnitType : 7; // CSSPrimitiveValue::UnitTypes
    mutable unsigned m_hasCachedCSSText : 1;
    unsigned m_isQuirkValue : 1;

    unsigned m_valueListSeparator : ValueListSeparatorBits;

private:
    unsigned m_classType : ClassTypeBits; // ClassType
};

template<typename CSSValueType>
inline bool compareCSSValueVector(const Vector<RefPtr<CSSValueType> >& firstVector, const Vector<RefPtr<CSSValueType> >& secondVector)
{
    size_t size = firstVector.size();
    if (size != secondVector.size())
        return false;

    for (size_t i = 0; i < size; i++) {
        const RefPtr<CSSValueType>& firstPtr = firstVector[i];
        const RefPtr<CSSValueType>& secondPtr = secondVector[i];
        if (firstPtr == secondPtr || (firstPtr && secondPtr && firstPtr->equals(*secondPtr)))
            continue;
        return false;
    }
    return true;
}

template<typename CSSValueType>
inline bool compareCSSValuePtr(const RefPtr<CSSValueType>& first, const RefPtr<CSSValueType>& second)
{
    return first ? second && first->equals(*second) : !second;
}

#define DEFINE_CSS_VALUE_TYPE_CASTS(ValueTypeName) \
inline const CSS##ValueTypeName* toCSS##ValueTypeName(const CSSValue* value) \
{ \
    ASSERT_WITH_SECURITY_IMPLICATION(!value || value->is##ValueTypeName()); \
    return static_cast<const CSS##ValueTypeName*>(value); \
} \
inline CSS##ValueTypeName* toCSS##ValueTypeName(CSSValue* value) \
{ \
    ASSERT_WITH_SECURITY_IMPLICATION(!value || value->is##ValueTypeName()); \
    return static_cast<CSS##ValueTypeName*>(value); \
} \
void toCSS##ValueTypeName(const CSS##ValueTypeName*); \
void toCSS##ValueTypeName(const CSS##ValueTypeName&)

} // namespace WebCore

#endif // CSSValue_h
