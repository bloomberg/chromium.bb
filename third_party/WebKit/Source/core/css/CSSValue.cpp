/*
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "core/css/CSSValue.h"

#include "core/css/CSSArrayFunctionValue.h"
#include "core/css/CSSAspectRatioValue.h"
#include "core/css/CSSBorderImageSliceValue.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSCanvasValue.h"
#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSFilterValue.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSGridTemplateValue.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSInheritedValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSLineBoxContainValue.h"
#include "core/css/CSSMixFunctionValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSSVGDocumentValue.h"
#include "core/css/CSSShaderValue.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSTransformValue.h"
#include "core/css/CSSUnicodeRangeValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSVariableValue.h"
#include "core/css/FontFeatureValue.h"
#include "core/css/FontValue.h"
#include "core/css/ShadowValue.h"
#include "core/svg/SVGColor.h"
#include "core/svg/SVGPaint.h"

namespace WebCore {

struct SameSizeAsCSSValue : public RefCounted<SameSizeAsCSSValue> {
    uint32_t bitfields;
};

COMPILE_ASSERT(sizeof(CSSValue) == sizeof(SameSizeAsCSSValue), CSS_value_should_stay_small);

class TextCloneCSSValue : public CSSValue {
public:
    static PassRefPtr<TextCloneCSSValue> create(ClassType classType, const String& text) { return adoptRef(new TextCloneCSSValue(classType, text)); }

    String cssText() const { return m_cssText; }

private:
    TextCloneCSSValue(ClassType classType, const String& text)
        : CSSValue(classType, /*isCSSOMSafe*/ true)
        , m_cssText(text)
    {
        m_isTextClone = true;
    }

    String m_cssText;
};

bool CSSValue::isImplicitInitialValue() const
{
    return m_classType == InitialClass && static_cast<const CSSInitialValue*>(this)->isImplicit();
}

CSSValue::Type CSSValue::cssValueType() const
{
    if (isInheritedValue())
        return CSS_INHERIT;
    if (isPrimitiveValue())
        return CSS_PRIMITIVE_VALUE;
    if (isValueList())
        return CSS_VALUE_LIST;
    if (isInitialValue())
        return CSS_INITIAL;
    return CSS_CUSTOM;
}

void CSSValue::addSubresourceStyleURLs(ListHashSet<KURL>& urls, const StyleSheetContents* styleSheet) const
{
    // This should get called for internal instances only.
    ASSERT(!isCSSOMSafe());

    if (isPrimitiveValue())
        toCSSPrimitiveValue(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (isValueList())
        toCSSValueList(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (classType() == FontFaceSrcClass)
        static_cast<const CSSFontFaceSrcValue*>(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (classType() == ReflectClass)
        static_cast<const CSSReflectValue*>(this)->addSubresourceStyleURLs(urls, styleSheet);
}

bool CSSValue::hasFailedOrCanceledSubresources() const
{
    // This should get called for internal instances only.
    ASSERT(!isCSSOMSafe());

    if (isValueList())
        return toCSSValueList(this)->hasFailedOrCanceledSubresources();
    if (classType() == FontFaceSrcClass)
        return static_cast<const CSSFontFaceSrcValue*>(this)->hasFailedOrCanceledSubresources();
    if (classType() == ImageClass)
        return toCSSImageValue(this)->hasFailedOrCanceledSubresources();
    if (classType() == CrossfadeClass)
        return static_cast<const CSSCrossfadeValue*>(this)->hasFailedOrCanceledSubresources();
    if (classType() == ImageSetClass)
        return static_cast<const CSSImageSetValue*>(this)->hasFailedOrCanceledSubresources();

    return false;
}

template<class ChildClassType>
inline static bool compareCSSValues(const CSSValue& first, const CSSValue& second)
{
    return static_cast<const ChildClassType&>(first).equals(static_cast<const ChildClassType&>(second));
}

bool CSSValue::equals(const CSSValue& other) const
{
    if (m_isTextClone) {
        ASSERT(isCSSOMSafe());
        return static_cast<const TextCloneCSSValue*>(this)->cssText() == other.cssText();
    }

    if (m_classType == other.m_classType) {
        switch (m_classType) {
        case AspectRatioClass:
            return compareCSSValues<CSSAspectRatioValue>(*this, other);
        case BorderImageSliceClass:
            return compareCSSValues<CSSBorderImageSliceValue>(*this, other);
        case CanvasClass:
            return compareCSSValues<CSSCanvasValue>(*this, other);
        case CursorImageClass:
            return compareCSSValues<CSSCursorImageValue>(*this, other);
        case FontClass:
            return compareCSSValues<FontValue>(*this, other);
        case FontFaceSrcClass:
            return compareCSSValues<CSSFontFaceSrcValue>(*this, other);
        case FontFeatureClass:
            return compareCSSValues<FontFeatureValue>(*this, other);
        case FunctionClass:
            return compareCSSValues<CSSFunctionValue>(*this, other);
        case LinearGradientClass:
            return compareCSSValues<CSSLinearGradientValue>(*this, other);
        case RadialGradientClass:
            return compareCSSValues<CSSRadialGradientValue>(*this, other);
        case CrossfadeClass:
            return compareCSSValues<CSSCrossfadeValue>(*this, other);
        case ImageClass:
            return compareCSSValues<CSSImageValue>(*this, other);
        case InheritedClass:
            return compareCSSValues<CSSInheritedValue>(*this, other);
        case InitialClass:
            return compareCSSValues<CSSInitialValue>(*this, other);
        case GridTemplateClass:
            return compareCSSValues<CSSGridTemplateValue>(*this, other);
        case PrimitiveClass:
            return compareCSSValues<CSSPrimitiveValue>(*this, other);
        case ReflectClass:
            return compareCSSValues<CSSReflectValue>(*this, other);
        case ShadowClass:
            return compareCSSValues<ShadowValue>(*this, other);
        case CubicBezierTimingFunctionClass:
            return compareCSSValues<CSSCubicBezierTimingFunctionValue>(*this, other);
        case StepsTimingFunctionClass:
            return compareCSSValues<CSSStepsTimingFunctionValue>(*this, other);
        case UnicodeRangeClass:
            return compareCSSValues<CSSUnicodeRangeValue>(*this, other);
        case ValueListClass:
            return compareCSSValues<CSSValueList>(*this, other);
        case CSSTransformClass:
            return compareCSSValues<CSSTransformValue>(*this, other);
        case LineBoxContainClass:
            return compareCSSValues<CSSLineBoxContainValue>(*this, other);
        case CalculationClass:
            return compareCSSValues<CSSCalcValue>(*this, other);
        case ImageSetClass:
            return compareCSSValues<CSSImageSetValue>(*this, other);
        case CSSFilterClass:
            return compareCSSValues<CSSFilterValue>(*this, other);
        case CSSArrayFunctionValueClass:
            return compareCSSValues<CSSArrayFunctionValue>(*this, other);
        case CSSMixFunctionValueClass:
            return compareCSSValues<CSSMixFunctionValue>(*this, other);
        case CSSShaderClass:
            return compareCSSValues<CSSShaderValue>(*this, other);
        case VariableClass:
            return compareCSSValues<CSSVariableValue>(*this, other);
        case SVGColorClass:
            return compareCSSValues<SVGColor>(*this, other);
        case SVGPaintClass:
            return compareCSSValues<SVGPaint>(*this, other);
        case CSSSVGDocumentClass:
            return compareCSSValues<CSSSVGDocumentValue>(*this, other);
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    } else if (m_classType == ValueListClass && other.m_classType != ValueListClass)
        return toCSSValueList(this)->equals(other);
    else if (m_classType != ValueListClass && other.m_classType == ValueListClass)
        return static_cast<const CSSValueList&>(other).equals(*this);
    return false;
}

String CSSValue::cssText() const
{
    if (m_isTextClone) {
         ASSERT(isCSSOMSafe());
        return static_cast<const TextCloneCSSValue*>(this)->cssText();
    }
    ASSERT(!isCSSOMSafe() || isSubtypeExposedToCSSOM());

    switch (classType()) {
    case AspectRatioClass:
        return static_cast<const CSSAspectRatioValue*>(this)->customCssText();
    case BorderImageSliceClass:
        return static_cast<const CSSBorderImageSliceValue*>(this)->customCssText();
    case CanvasClass:
        return static_cast<const CSSCanvasValue*>(this)->customCssText();
    case CursorImageClass:
        return static_cast<const CSSCursorImageValue*>(this)->customCssText();
    case FontClass:
        return static_cast<const FontValue*>(this)->customCssText();
    case FontFaceSrcClass:
        return static_cast<const CSSFontFaceSrcValue*>(this)->customCssText();
    case FontFeatureClass:
        return static_cast<const FontFeatureValue*>(this)->customCssText();
    case FunctionClass:
        return static_cast<const CSSFunctionValue*>(this)->customCssText();
    case LinearGradientClass:
        return static_cast<const CSSLinearGradientValue*>(this)->customCssText();
    case RadialGradientClass:
        return static_cast<const CSSRadialGradientValue*>(this)->customCssText();
    case CrossfadeClass:
        return static_cast<const CSSCrossfadeValue*>(this)->customCssText();
    case ImageClass:
        return toCSSImageValue(this)->customCssText();
    case InheritedClass:
        return static_cast<const CSSInheritedValue*>(this)->customCssText();
    case InitialClass:
        return static_cast<const CSSInitialValue*>(this)->customCssText();
    case GridTemplateClass:
        return static_cast<const CSSGridTemplateValue*>(this)->customCssText();
    case PrimitiveClass:
        return toCSSPrimitiveValue(this)->customCssText();
    case ReflectClass:
        return static_cast<const CSSReflectValue*>(this)->customCssText();
    case ShadowClass:
        return static_cast<const ShadowValue*>(this)->customCssText();
    case CubicBezierTimingFunctionClass:
        return static_cast<const CSSCubicBezierTimingFunctionValue*>(this)->customCssText();
    case StepsTimingFunctionClass:
        return static_cast<const CSSStepsTimingFunctionValue*>(this)->customCssText();
    case UnicodeRangeClass:
        return static_cast<const CSSUnicodeRangeValue*>(this)->customCssText();
    case ValueListClass:
        return toCSSValueList(this)->customCssText();
    case CSSTransformClass:
        return static_cast<const CSSTransformValue*>(this)->customCssText();
    case LineBoxContainClass:
        return static_cast<const CSSLineBoxContainValue*>(this)->customCssText();
    case CalculationClass:
        return static_cast<const CSSCalcValue*>(this)->customCssText();
    case ImageSetClass:
        return static_cast<const CSSImageSetValue*>(this)->customCssText();
    case CSSFilterClass:
        return static_cast<const CSSFilterValue*>(this)->customCssText();
    case CSSArrayFunctionValueClass:
        return static_cast<const CSSArrayFunctionValue*>(this)->customCssText();
    case CSSMixFunctionValueClass:
        return static_cast<const CSSMixFunctionValue*>(this)->customCssText();
    case CSSShaderClass:
        return static_cast<const CSSShaderValue*>(this)->customCssText();
    case VariableClass:
        return toCSSVariableValue(this)->value();
    case SVGColorClass:
        return static_cast<const SVGColor*>(this)->customCssText();
    case SVGPaintClass:
        return static_cast<const SVGPaint*>(this)->customCssText();
    case CSSSVGDocumentClass:
        return static_cast<const CSSSVGDocumentValue*>(this)->customCssText();
    }
    ASSERT_NOT_REACHED();
    return String();
}

String CSSValue::serializeResolvingVariables(const HashMap<AtomicString, String>& variables) const
{
    switch (classType()) {
    case PrimitiveClass:
        return toCSSPrimitiveValue(this)->customSerializeResolvingVariables(variables);
    case ReflectClass:
        return static_cast<const CSSReflectValue*>(this)->customSerializeResolvingVariables(variables);
    case ValueListClass:
        return toCSSValueList(this)->customSerializeResolvingVariables(variables);
    case CSSTransformClass:
        return static_cast<const CSSTransformValue*>(this)->customSerializeResolvingVariables(variables);
    default:
        return cssText();
    }
}

void CSSValue::destroy()
{
    if (m_isTextClone) {
        ASSERT(isCSSOMSafe());
        delete static_cast<TextCloneCSSValue*>(this);
        return;
    }
    ASSERT(!isCSSOMSafe() || isSubtypeExposedToCSSOM());

    switch (classType()) {
    case AspectRatioClass:
        delete toCSSAspectRatioValue(this);
        return;
    case BorderImageSliceClass:
        delete toCSSBorderImageSliceValue(this);
        return;
    case CanvasClass:
        delete static_cast<CSSCanvasValue*>(this);
        return;
    case CursorImageClass:
        delete toCSSCursorImageValue(this);
        return;
    case FontClass:
        delete static_cast<FontValue*>(this);
        return;
    case FontFaceSrcClass:
        delete toCSSFontFaceSrcValue(this);
        return;
    case FontFeatureClass:
        delete static_cast<FontFeatureValue*>(this);
        return;
    case FunctionClass:
        delete static_cast<CSSFunctionValue*>(this);
        return;
    case LinearGradientClass:
        delete static_cast<CSSLinearGradientValue*>(this);
        return;
    case RadialGradientClass:
        delete static_cast<CSSRadialGradientValue*>(this);
        return;
    case CrossfadeClass:
        delete static_cast<CSSCrossfadeValue*>(this);
        return;
    case ImageClass:
        delete toCSSImageValue(this);
        return;
    case InheritedClass:
        delete static_cast<CSSInheritedValue*>(this);
        return;
    case InitialClass:
        delete static_cast<CSSInitialValue*>(this);
        return;
    case GridTemplateClass:
        delete static_cast<CSSGridTemplateValue*>(this);
        return;
    case PrimitiveClass:
        delete toCSSPrimitiveValue(this);
        return;
    case ReflectClass:
        delete toCSSReflectValue(this);
        return;
    case ShadowClass:
        delete static_cast<ShadowValue*>(this);
        return;
    case CubicBezierTimingFunctionClass:
        delete static_cast<CSSCubicBezierTimingFunctionValue*>(this);
        return;
    case StepsTimingFunctionClass:
        delete static_cast<CSSStepsTimingFunctionValue*>(this);
        return;
    case UnicodeRangeClass:
        delete static_cast<CSSUnicodeRangeValue*>(this);
        return;
    case ValueListClass:
        delete toCSSValueList(this);
        return;
    case CSSTransformClass:
        delete static_cast<CSSTransformValue*>(this);
        return;
    case LineBoxContainClass:
        delete static_cast<CSSLineBoxContainValue*>(this);
        return;
    case CalculationClass:
        delete static_cast<CSSCalcValue*>(this);
        return;
    case ImageSetClass:
        delete toCSSImageSetValue(this);
        return;
    case CSSFilterClass:
        delete static_cast<CSSFilterValue*>(this);
        return;
    case CSSArrayFunctionValueClass:
        delete static_cast<CSSArrayFunctionValue*>(this);
        return;
    case CSSMixFunctionValueClass:
        delete static_cast<CSSMixFunctionValue*>(this);
        return;
    case CSSShaderClass:
        delete static_cast<CSSShaderValue*>(this);
        return;
    case VariableClass:
        delete toCSSVariableValue(this);
        return;
    case SVGColorClass:
        delete static_cast<SVGColor*>(this);
        return;
    case SVGPaintClass:
        delete static_cast<SVGPaint*>(this);
        return;
    case CSSSVGDocumentClass:
        delete static_cast<CSSSVGDocumentValue*>(this);
        return;
    }
    ASSERT_NOT_REACHED();
}

PassRefPtr<CSSValue> CSSValue::cloneForCSSOM() const
{
    switch (classType()) {
    case PrimitiveClass:
        return toCSSPrimitiveValue(this)->cloneForCSSOM();
    case ValueListClass:
        return toCSSValueList(this)->cloneForCSSOM();
    case ImageClass:
    case CursorImageClass:
        return toCSSImageValue(this)->cloneForCSSOM();
    case CSSFilterClass:
        return static_cast<const CSSFilterValue*>(this)->cloneForCSSOM();
    case CSSArrayFunctionValueClass:
        return static_cast<const CSSArrayFunctionValue*>(this)->cloneForCSSOM();
    case CSSMixFunctionValueClass:
        return static_cast<const CSSMixFunctionValue*>(this)->cloneForCSSOM();
    case CSSTransformClass:
        return static_cast<const CSSTransformValue*>(this)->cloneForCSSOM();
    case ImageSetClass:
        return static_cast<const CSSImageSetValue*>(this)->cloneForCSSOM();
    case SVGColorClass:
        return static_cast<const SVGColor*>(this)->cloneForCSSOM();
    case SVGPaintClass:
        return static_cast<const SVGPaint*>(this)->cloneForCSSOM();
    default:
        ASSERT(!isSubtypeExposedToCSSOM());
        return TextCloneCSSValue::create(classType(), cssText());
    }
}

}
