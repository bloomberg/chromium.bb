/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/DeprecatedStyleBuilder.h"

#include "core/css/BasicShapeFunctions.h"
#include "core/css/CSSAspectRatioValue.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSToStyleMap.h"
#include "core/css/CSSValueList.h"
#include "core/css/FontSize.h"
#include "core/css/Pair.h"
#include "core/css/Rect.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/page/Settings.h"
#include "core/rendering/ClipPathOperation.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/style/BasicShapes.h"
#include "core/rendering/style/CursorList.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/ShapeValue.h"
#include "wtf/StdLibExtras.h"
#include "wtf/UnusedParam.h"

using namespace std;

namespace WebCore {

enum ExpandValueBehavior {SuppressValue = 0, ExpandValue};
template <ExpandValueBehavior expandValue, CSSPropertyID one = CSSPropertyInvalid, CSSPropertyID two = CSSPropertyInvalid, CSSPropertyID three = CSSPropertyInvalid, CSSPropertyID four = CSSPropertyInvalid, CSSPropertyID five = CSSPropertyInvalid>
class ApplyPropertyExpanding {
public:

    template <CSSPropertyID id>
    static inline void applyInheritValue(CSSPropertyID propertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        if (id == CSSPropertyInvalid)
            return;

        const DeprecatedStyleBuilder& table = DeprecatedStyleBuilder::sharedStyleBuilder();
        const PropertyHandler& handler = table.propertyHandler(id);
        if (handler.isValid())
            handler.applyInheritValue(propertyID, styleResolver, state);
    }

    static void applyInheritValue(CSSPropertyID propertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        applyInheritValue<one>(propertyID, styleResolver, state);
        applyInheritValue<two>(propertyID, styleResolver, state);
        applyInheritValue<three>(propertyID, styleResolver, state);
        applyInheritValue<four>(propertyID, styleResolver, state);
        applyInheritValue<five>(propertyID, styleResolver, state);
    }

    template <CSSPropertyID id>
    static inline void applyInitialValue(CSSPropertyID propertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        if (id == CSSPropertyInvalid)
            return;

        const DeprecatedStyleBuilder& table = DeprecatedStyleBuilder::sharedStyleBuilder();
        const PropertyHandler& handler = table.propertyHandler(id);
        if (handler.isValid())
            handler.applyInitialValue(propertyID, styleResolver, state);
    }

    static void applyInitialValue(CSSPropertyID propertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        applyInitialValue<one>(propertyID, styleResolver, state);
        applyInitialValue<two>(propertyID, styleResolver, state);
        applyInitialValue<three>(propertyID, styleResolver, state);
        applyInitialValue<four>(propertyID, styleResolver, state);
        applyInitialValue<five>(propertyID, styleResolver, state);
    }

    template <CSSPropertyID id>
    static inline void applyValue(CSSPropertyID propertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        if (id == CSSPropertyInvalid)
            return;

        const DeprecatedStyleBuilder& table = DeprecatedStyleBuilder::sharedStyleBuilder();
        const PropertyHandler& handler = table.propertyHandler(id);
        if (handler.isValid())
            handler.applyValue(propertyID, styleResolver, state, value);
    }

    static void applyValue(CSSPropertyID propertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        if (!expandValue)
            return;

        applyValue<one>(propertyID, styleResolver, state, value);
        applyValue<two>(propertyID, styleResolver, state, value);
        applyValue<three>(propertyID, styleResolver, state, value);
        applyValue<four>(propertyID, styleResolver, state, value);
        applyValue<five>(propertyID, styleResolver, state, value);
    }
    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

template <typename GetterType, GetterType (RenderStyle::*getterFunction)() const, typename SetterType, void (RenderStyle::*setterFunction)(SetterType), typename InitialType, InitialType (*initialFunction)()>
class ApplyPropertyDefaultBase {
public:
    static void setValue(RenderStyle* style, SetterType value) { (style->*setterFunction)(value); }
    static GetterType value(RenderStyle* style) { return (style->*getterFunction)(); }
    static InitialType initial() { return (*initialFunction)(); }
    static void applyInheritValue(CSSPropertyID, StyleResolver*, StyleResolverState& state) { setValue(state.style(), value(state.parentStyle())); }
    static void applyInitialValue(CSSPropertyID, StyleResolver*, StyleResolverState& state) { setValue(state.style(), initial()); }
    static void applyValue(CSSPropertyID, StyleResolver*, StyleResolverState&, CSSValue*) { }
    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

template <typename GetterType, GetterType (RenderStyle::*getterFunction)() const, typename SetterType, void (RenderStyle::*setterFunction)(SetterType), typename InitialType, InitialType (*initialFunction)()>
class ApplyPropertyDefault {
public:
    static void setValue(RenderStyle* style, SetterType value) { (style->*setterFunction)(value); }
    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        if (value->isPrimitiveValue())
            setValue(state.style(), *toCSSPrimitiveValue(value));
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<GetterType, getterFunction, SetterType, setterFunction, InitialType, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

template <typename NumberType, NumberType (RenderStyle::*getterFunction)() const, void (RenderStyle::*setterFunction)(NumberType), NumberType (*initialFunction)(), CSSValueID idMapsToMinusOne = CSSValueAuto>
class ApplyPropertyNumber {
public:
    static void setValue(RenderStyle* style, NumberType value) { (style->*setterFunction)(value); }
    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        if (primitiveValue->getValueID() == idMapsToMinusOne)
            setValue(state.style(), -1);
        else
            setValue(state.style(), primitiveValue->getValue<NumberType>(CSSPrimitiveValue::CSS_NUMBER));
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<NumberType, getterFunction, NumberType, setterFunction, NumberType, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

template <StyleImage* (RenderStyle::*getterFunction)() const, void (RenderStyle::*setterFunction)(PassRefPtr<StyleImage>), StyleImage* (*initialFunction)(), CSSPropertyID property>
class ApplyPropertyStyleImage {
public:
    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value) { (state.style()->*setterFunction)(state.styleImage(property, value)); }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<StyleImage*, getterFunction, PassRefPtr<StyleImage>, setterFunction, StyleImage*, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

class ApplyPropertyClip {
private:
    static Length convertToLength(StyleResolverState& state, CSSPrimitiveValue* value)
    {
        return value->convertToLength<FixedIntegerConversion | PercentConversion | FractionConversion | AutoConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom());
    }
public:
    static void applyInheritValue(CSSPropertyID propertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        RenderStyle* parentStyle = state.parentStyle();
        if (!parentStyle->hasClip())
            return applyInitialValue(propertyID, styleResolver, state);
        state.style()->setClip(parentStyle->clipTop(), parentStyle->clipRight(), parentStyle->clipBottom(), parentStyle->clipLeft());
        state.style()->setHasClip(true);
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        state.style()->setClip(Length(), Length(), Length(), Length());
        state.style()->setHasClip(false);
    }

    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

        if (Rect* rect = primitiveValue->getRectValue()) {
            Length top = convertToLength(state, rect->top());
            Length right = convertToLength(state, rect->right());
            Length bottom = convertToLength(state, rect->bottom());
            Length left = convertToLength(state, rect->left());
            state.style()->setClip(top, right, bottom, left);
            state.style()->setHasClip(true);
        } else if (primitiveValue->getValueID() == CSSValueAuto) {
            state.style()->setClip(Length(), Length(), Length(), Length());
            state.style()->setHasClip(false);
        }
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

template <TextDirection (RenderStyle::*getterFunction)() const, void (RenderStyle::*setterFunction)(TextDirection), TextDirection (*initialFunction)()>
class ApplyPropertyDirection {
public:
    static void applyValue(CSSPropertyID propertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        ApplyPropertyDefault<TextDirection, getterFunction, TextDirection, setterFunction, TextDirection, initialFunction>::applyValue(propertyID, styleResolver, state, value);
        Element* element = state.element();
        if (element && state.element() == element->document()->documentElement())
            element->document()->setDirectionSetOnDocumentElement(true);
    }

    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefault<TextDirection, getterFunction, TextDirection, setterFunction, TextDirection, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

enum ComputeLengthNormal {NormalDisabled = 0, NormalEnabled};
enum ComputeLengthThickness {ThicknessDisabled = 0, ThicknessEnabled};
enum ComputeLengthSVGZoom {SVGZoomDisabled = 0, SVGZoomEnabled};
template <typename T,
          T (RenderStyle::*getterFunction)() const,
          void (RenderStyle::*setterFunction)(T),
          T (*initialFunction)(),
          ComputeLengthNormal normalEnabled = NormalDisabled,
          ComputeLengthThickness thicknessEnabled = ThicknessDisabled,
          ComputeLengthSVGZoom svgZoomEnabled = SVGZoomDisabled>
class ApplyPropertyComputeLength {
public:
    static void setValue(RenderStyle* style, T value) { (style->*setterFunction)(value); }
    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        // note: CSSPropertyLetter/WordSpacing right now sets to zero if it's not a primitive value for some reason...
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

        CSSValueID valueID = primitiveValue->getValueID();
        T length;
        if (normalEnabled && valueID == CSSValueNormal) {
            length = 0;
        } else if (thicknessEnabled && valueID == CSSValueThin) {
            length = 1;
        } else if (thicknessEnabled && valueID == CSSValueMedium) {
            length = 3;
        } else if (thicknessEnabled && valueID == CSSValueThick) {
            length = 5;
        } else if (valueID == CSSValueInvalid) {
            float zoom = (svgZoomEnabled && state.useSVGZoomRules()) ? 1.0f : state.style()->effectiveZoom();

            // Any original result that was >= 1 should not be allowed to fall below 1.
            // This keeps border lines from vanishing.
            length = primitiveValue->computeLength<T>(state.style(), state.rootElementStyle(), zoom);
            if (zoom < 1.0f && length < 1.0) {
                T originalLength = primitiveValue->computeLength<T>(state.style(), state.rootElementStyle(), 1.0);
                if (originalLength >= 1.0)
                    length = 1.0;
            }

        } else {
            ASSERT_NOT_REACHED();
            length = 0;
        }

        setValue(state.style(), length);
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<T, getterFunction, T, setterFunction, T, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

template <typename T, T (FontDescription::*getterFunction)() const, void (FontDescription::*setterFunction)(T), T initialValue>
class ApplyPropertyFont {
public:
    static void applyInheritValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        FontDescription fontDescription = state.fontDescription();
        (fontDescription.*setterFunction)((state.parentFontDescription().*getterFunction)());
        state.setFontDescription(fontDescription);
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        FontDescription fontDescription = state.fontDescription();
        (fontDescription.*setterFunction)(initialValue);
        state.setFontDescription(fontDescription);
    }

    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        FontDescription fontDescription = state.fontDescription();
        (fontDescription.*setterFunction)(*primitiveValue);
        state.setFontDescription(fontDescription);
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

class ApplyPropertyFontFamily {
public:
    static void applyInheritValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        FontDescription fontDescription = state.style()->fontDescription();
        FontDescription parentFontDescription = state.parentStyle()->fontDescription();
        
        fontDescription.setGenericFamily(parentFontDescription.genericFamily());
        fontDescription.setFamily(parentFontDescription.firstFamily());
        fontDescription.setIsSpecifiedFont(parentFontDescription.isSpecifiedFont());
        state.setFontDescription(fontDescription);
        return;
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        FontDescription fontDescription = state.style()->fontDescription();
        FontDescription initialDesc = FontDescription();
        
        // We need to adjust the size to account for the generic family change from monospace to non-monospace.
        if (fontDescription.keywordSize() && fontDescription.useFixedDefaultSize())
            styleResolver->setFontSize(fontDescription, FontSize::fontSizeForKeyword(styleResolver->document(), CSSValueXxSmall + fontDescription.keywordSize() - 1, false));
        fontDescription.setGenericFamily(initialDesc.genericFamily());
        if (!initialDesc.firstFamily().familyIsEmpty())
            fontDescription.setFamily(initialDesc.firstFamily());

        state.setFontDescription(fontDescription);
        return;
    }

    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isValueList())
            return;

        FontDescription fontDescription = state.style()->fontDescription();
        FontFamily& firstFamily = fontDescription.firstFamily();
        FontFamily* currFamily = 0;

        // Before mapping in a new font-family property, we should reset the generic family.
        bool oldFamilyUsedFixedDefaultSize = fontDescription.useFixedDefaultSize();
        fontDescription.setGenericFamily(FontDescription::NoFamily);

        for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
            CSSValue* item = i.value();
            if (!item->isPrimitiveValue())
                continue;
            CSSPrimitiveValue* contentValue = toCSSPrimitiveValue(item);
            AtomicString face;
            Settings* settings = styleResolver->document()->settings();
            if (contentValue->isString())
                face = contentValue->getStringValue();
            else if (settings) {
                switch (contentValue->getValueID()) {
                case CSSValueWebkitBody:
                    face = settings->standardFontFamily();
                    break;
                case CSSValueSerif:
                    face = serifFamily;
                    fontDescription.setGenericFamily(FontDescription::SerifFamily);
                    break;
                case CSSValueSansSerif:
                    face = sansSerifFamily;
                    fontDescription.setGenericFamily(FontDescription::SansSerifFamily);
                    break;
                case CSSValueCursive:
                    face = cursiveFamily;
                    fontDescription.setGenericFamily(FontDescription::CursiveFamily);
                    break;
                case CSSValueFantasy:
                    face = fantasyFamily;
                    fontDescription.setGenericFamily(FontDescription::FantasyFamily);
                    break;
                case CSSValueMonospace:
                    face = monospaceFamily;
                    fontDescription.setGenericFamily(FontDescription::MonospaceFamily);
                    break;
                case CSSValueWebkitPictograph:
                    face = pictographFamily;
                    fontDescription.setGenericFamily(FontDescription::PictographFamily);
                    break;
                default:
                    break;
                }
            }

            if (!face.isEmpty()) {
                if (!currFamily) {
                    // Filling in the first family.
                    firstFamily.setFamily(face);
                    firstFamily.appendFamily(0); // Remove any inherited family-fallback list.
                    currFamily = &firstFamily;
                    fontDescription.setIsSpecifiedFont(fontDescription.genericFamily() == FontDescription::NoFamily);
                } else {
                    RefPtr<SharedFontFamily> newFamily = SharedFontFamily::create();
                    newFamily->setFamily(face);
                    currFamily->appendFamily(newFamily);
                    currFamily = newFamily.get();
                }
            }
        }

        // We can't call useFixedDefaultSize() until all new font families have been added
        // If currFamily is non-zero then we set at least one family on this description.
        if (currFamily) {
            if (fontDescription.keywordSize() && fontDescription.useFixedDefaultSize() != oldFamilyUsedFixedDefaultSize)
                styleResolver->setFontSize(fontDescription, FontSize::fontSizeForKeyword(styleResolver->document(), CSSValueXxSmall + fontDescription.keywordSize() - 1, !oldFamilyUsedFixedDefaultSize));

            state.setFontDescription(fontDescription);
        }
        return;
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

class ApplyPropertyFontSize {
private:
    // When the CSS keyword "larger" is used, this function will attempt to match within the keyword
    // table, and failing that, will simply multiply by 1.2.
    static float largerFontSize(float size)
    {
        // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale up to
        // the next size level.
        return size * 1.2f;
    }

    // Like the previous function, but for the keyword "smaller".
    static float smallerFontSize(float size)
    {
        // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale down to
        // the next size level.
        return size / 1.2f;
    }
public:
    static void applyInheritValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        float size = state.parentStyle()->fontDescription().specifiedSize();

        if (size < 0)
            return;

        FontDescription fontDescription = state.style()->fontDescription();
        fontDescription.setKeywordSize(state.parentStyle()->fontDescription().keywordSize());
        styleResolver->setFontSize(fontDescription, size);
        state.setFontDescription(fontDescription);
        return;
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        FontDescription fontDescription = state.style()->fontDescription();
        float size = FontSize::fontSizeForKeyword(styleResolver->document(), CSSValueMedium, fontDescription.useFixedDefaultSize());

        if (size < 0)
            return;

        fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
        styleResolver->setFontSize(fontDescription, size);
        state.setFontDescription(fontDescription);
        return;
    }

    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

        FontDescription fontDescription = state.style()->fontDescription();
        fontDescription.setKeywordSize(0);
        float parentSize = 0;
        bool parentIsAbsoluteSize = false;
        float size = 0;

        if (state.parentStyle()) {
            parentSize = state.parentStyle()->fontDescription().specifiedSize();
            parentIsAbsoluteSize = state.parentStyle()->fontDescription().isAbsoluteSize();
        }

        if (CSSValueID valueID = primitiveValue->getValueID()) {
            // Keywords are being used.
            switch (valueID) {
            case CSSValueXxSmall:
            case CSSValueXSmall:
            case CSSValueSmall:
            case CSSValueMedium:
            case CSSValueLarge:
            case CSSValueXLarge:
            case CSSValueXxLarge:
            case CSSValueWebkitXxxLarge:
                size = FontSize::fontSizeForKeyword(styleResolver->document(), valueID, fontDescription.useFixedDefaultSize());
                fontDescription.setKeywordSize(valueID - CSSValueXxSmall + 1);
                break;
            case CSSValueLarger:
                size = largerFontSize(parentSize);
                break;
            case CSSValueSmaller:
                size = smallerFontSize(parentSize);
                break;
            default:
                return;
            }

            fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize && (valueID == CSSValueLarger || valueID == CSSValueSmaller));
        } else {
            fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize
                                              || !(primitiveValue->isPercentage() || primitiveValue->isFontRelativeLength()));
            if (primitiveValue->isLength())
                size = primitiveValue->computeLength<float>(state.parentStyle(), state.rootElementStyle(), 1.0, true);
            else if (primitiveValue->isPercentage())
                size = (primitiveValue->getFloatValue() * parentSize) / 100.0f;
            else if (primitiveValue->isCalculatedPercentageWithLength())
                size = primitiveValue->cssCalcValue()->toCalcValue(state.parentStyle(), state.rootElementStyle())->evaluate(parentSize);
            else if (primitiveValue->isViewportPercentageLength())
                size = valueForLength(primitiveValue->viewportPercentageLength(), 0, styleResolver->document()->renderView());
            else
                return;
        }

        if (size < 0)
            return;

        // Overly large font sizes will cause crashes on some platforms (such as Windows).
        // Cap font size here to make sure that doesn't happen.
        size = min(maximumAllowedFontSize, size);

        styleResolver->setFontSize(fontDescription, size);
        state.setFontDescription(fontDescription);
        return;
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

class ApplyPropertyFontWeight {
public:
    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        FontDescription fontDescription = state.fontDescription();
        switch (primitiveValue->getValueID()) {
        case CSSValueInvalid:
            ASSERT_NOT_REACHED();
            break;
        case CSSValueBolder:
            fontDescription.setWeight(fontDescription.bolderWeight());
            break;
        case CSSValueLighter:
            fontDescription.setWeight(fontDescription.lighterWeight());
            break;
        default:
            fontDescription.setWeight(*primitiveValue);
        }
        state.setFontDescription(fontDescription);
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyFont<FontWeight, &FontDescription::weight, &FontDescription::setWeight, FontWeightNormal>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

class ApplyPropertyFontVariantLigatures {
public:
    static void applyInheritValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        const FontDescription& parentFontDescription = state.parentFontDescription();
        FontDescription fontDescription = state.fontDescription();

        fontDescription.setCommonLigaturesState(parentFontDescription.commonLigaturesState());
        fontDescription.setDiscretionaryLigaturesState(parentFontDescription.discretionaryLigaturesState());
        fontDescription.setHistoricalLigaturesState(parentFontDescription.historicalLigaturesState());

        state.setFontDescription(fontDescription);
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        FontDescription fontDescription = state.fontDescription();

        fontDescription.setCommonLigaturesState(FontDescription::NormalLigaturesState);
        fontDescription.setDiscretionaryLigaturesState(FontDescription::NormalLigaturesState);
        fontDescription.setHistoricalLigaturesState(FontDescription::NormalLigaturesState);

        state.setFontDescription(fontDescription);
    }

    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        FontDescription::LigaturesState commonLigaturesState = FontDescription::NormalLigaturesState;
        FontDescription::LigaturesState discretionaryLigaturesState = FontDescription::NormalLigaturesState;
        FontDescription::LigaturesState historicalLigaturesState = FontDescription::NormalLigaturesState;

        if (value->isValueList()) {
            CSSValueList* valueList = toCSSValueList(value);
            for (size_t i = 0; i < valueList->length(); ++i) {
                CSSValue* item = valueList->itemWithoutBoundsCheck(i);
                ASSERT(item->isPrimitiveValue());
                if (item->isPrimitiveValue()) {
                    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(item);
                    switch (primitiveValue->getValueID()) {
                    case CSSValueNoCommonLigatures:
                        commonLigaturesState = FontDescription::DisabledLigaturesState;
                        break;
                    case CSSValueCommonLigatures:
                        commonLigaturesState = FontDescription::EnabledLigaturesState;
                        break;
                    case CSSValueNoDiscretionaryLigatures:
                        discretionaryLigaturesState = FontDescription::DisabledLigaturesState;
                        break;
                    case CSSValueDiscretionaryLigatures:
                        discretionaryLigaturesState = FontDescription::EnabledLigaturesState;
                        break;
                    case CSSValueNoHistoricalLigatures:
                        historicalLigaturesState = FontDescription::DisabledLigaturesState;
                        break;
                    case CSSValueHistoricalLigatures:
                        historicalLigaturesState = FontDescription::EnabledLigaturesState;
                        break;
                    default:
                        ASSERT_NOT_REACHED();
                        break;
                    }
                }
            }
        }
#if !ASSERT_DISABLED
        else {
            ASSERT_WITH_SECURITY_IMPLICATION(value->isPrimitiveValue());
            ASSERT(toCSSPrimitiveValue(value)->getValueID() == CSSValueNormal);
        }
#endif

        FontDescription fontDescription = state.fontDescription();
        fontDescription.setCommonLigaturesState(commonLigaturesState);
        fontDescription.setDiscretionaryLigaturesState(discretionaryLigaturesState);
        fontDescription.setHistoricalLigaturesState(historicalLigaturesState);
        state.setFontDescription(fontDescription);
    }

    static PropertyHandler createHandler()
    {
        return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue);
    }
};

enum CounterBehavior {Increment = 0, Reset};
template <CounterBehavior counterBehavior>
class ApplyPropertyCounter {
public:
    static void emptyFunction(CSSPropertyID, StyleResolver*, StyleResolverState&) { }
    static void applyInheritValue(CSSPropertyID, StyleResolver*, StyleResolverState& state)
    {
        CounterDirectiveMap& map = state.style()->accessCounterDirectives();
        CounterDirectiveMap& parentMap = state.parentStyle()->accessCounterDirectives();

        typedef CounterDirectiveMap::iterator Iterator;
        Iterator end = parentMap.end();
        for (Iterator it = parentMap.begin(); it != end; ++it) {
            CounterDirectives& directives = map.add(it->key, CounterDirectives()).iterator->value;
            if (counterBehavior == Reset)
                directives.inheritReset(it->value);
            else
                directives.inheritIncrement(it->value);
        }
    }
    static void applyValue(CSSPropertyID, StyleResolver*, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isValueList())
            return;

        CSSValueList* list = toCSSValueList(value);

        CounterDirectiveMap& map = state.style()->accessCounterDirectives();
        typedef CounterDirectiveMap::iterator Iterator;

        Iterator end = map.end();
        for (Iterator it = map.begin(); it != end; ++it)
            if (counterBehavior == Reset)
                it->value.clearReset();
            else
                it->value.clearIncrement();

        int length = list ? list->length() : 0;
        for (int i = 0; i < length; ++i) {
            CSSValue* currValue = list->itemWithoutBoundsCheck(i);
            if (!currValue->isPrimitiveValue())
                continue;

            Pair* pair = toCSSPrimitiveValue(currValue)->getPairValue();
            if (!pair || !pair->first() || !pair->second())
                continue;

            AtomicString identifier = pair->first()->getStringValue();
            int value = pair->second()->getIntValue();
            CounterDirectives& directives = map.add(identifier, CounterDirectives()).iterator->value;
            if (counterBehavior == Reset) {
                directives.setResetValue(value);
            } else {
                directives.addIncrementValue(value);
            }
        }
    }
    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &emptyFunction, &applyValue); }
};


class ApplyPropertyCursor {
public:
    static void applyInheritValue(CSSPropertyID, StyleResolver*, StyleResolverState& state)
    {
        state.style()->setCursor(state.parentStyle()->cursor());
        state.style()->setCursorList(state.parentStyle()->cursors());
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver*, StyleResolverState& state)
    {
        state.style()->clearCursorList();
        state.style()->setCursor(RenderStyle::initialCursor());
    }

    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        state.style()->clearCursorList();
        if (value->isValueList()) {
            CSSValueList* list = toCSSValueList(value);
            int len = list->length();
            state.style()->setCursor(CURSOR_AUTO);
            for (int i = 0; i < len; i++) {
                CSSValue* item = list->itemWithoutBoundsCheck(i);
                if (item->isCursorImageValue()) {
                    CSSCursorImageValue* image = static_cast<CSSCursorImageValue*>(item);
                    if (image->updateIfSVGCursorIsUsed(state.element())) // Elements with SVG cursors are not allowed to share style.
                        state.style()->setUnique();
                    state.style()->addCursor(state.styleImage(CSSPropertyCursor, image), image->hotSpot());
                } else if (item->isPrimitiveValue()) {
                    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(item);
                    if (primitiveValue->isValueID())
                        state.style()->setCursor(*primitiveValue);
                }
            }
        } else if (value->isPrimitiveValue()) {
            CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
            if (primitiveValue->isValueID() && state.style()->cursor() != ECursor(*primitiveValue))
                state.style()->setCursor(*primitiveValue);
        }
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

class ApplyPropertyTextAlign {
public:
    static void applyValue(CSSPropertyID, StyleResolver*, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

        if (primitiveValue->getValueID() != CSSValueWebkitMatchParent)
            state.style()->setTextAlign(*primitiveValue);
        else if (state.parentStyle()->textAlign() == TASTART)
            state.style()->setTextAlign(state.parentStyle()->isLeftToRightDirection() ? LEFT : RIGHT);
        else if (state.parentStyle()->textAlign() == TAEND)
            state.style()->setTextAlign(state.parentStyle()->isLeftToRightDirection() ? RIGHT : LEFT);
        else
            state.style()->setTextAlign(state.parentStyle()->textAlign());
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<ETextAlign, &RenderStyle::textAlign, ETextAlign, &RenderStyle::setTextAlign, ETextAlign, &RenderStyle::initialTextAlign>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

class ApplyPropertyTextDecoration {
public:
    static void applyValue(CSSPropertyID, StyleResolver*, StyleResolverState& state, CSSValue* value)
    {
        TextDecoration t = RenderStyle::initialTextDecoration();
        for (CSSValueListIterator i(value); i.hasMore(); i.advance()) {
            CSSValue* item = i.value();
            t |= *toCSSPrimitiveValue(item);
        }
        state.style()->setTextDecoration(t);
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<TextDecoration, &RenderStyle::textDecoration, TextDecoration, &RenderStyle::setTextDecoration, TextDecoration, &RenderStyle::initialTextDecoration>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

class ApplyPropertyMarqueeSpeed {
public:
    static void applyValue(CSSPropertyID, StyleResolver*, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        if (CSSValueID valueID = primitiveValue->getValueID()) {
            switch (valueID) {
            case CSSValueSlow:
                state.style()->setMarqueeSpeed(500); // 500 msec.
                break;
            case CSSValueNormal:
                state.style()->setMarqueeSpeed(85); // 85msec. The WinIE default.
                break;
            case CSSValueFast:
                state.style()->setMarqueeSpeed(10); // 10msec. Super fast.
                break;
            default:
                break;
            }
        } else if (primitiveValue->isTime()) {
            state.style()->setMarqueeSpeed(primitiveValue->computeTime<int, CSSPrimitiveValue::Milliseconds>());
        } else if (primitiveValue->isNumber()) { // For scrollamount support.
            state.style()->setMarqueeSpeed(primitiveValue->getIntValue());
        }
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefault<int, &RenderStyle::marqueeSpeed, int, &RenderStyle::setMarqueeSpeed, int, &RenderStyle::initialMarqueeSpeed>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

#if ENABLE(CSS3_TEXT)
class ApplyPropertyTextUnderlinePosition {
public:
    static void applyValue(CSSPropertyID, StyleResolver*, StyleResolverState& state, CSSValue* value)
    {
        // This is true if value is 'auto' or 'alphabetic'.
        if (value->isPrimitiveValue()) {
            TextUnderlinePosition t = *toCSSPrimitiveValue(value);
            state.style()->setTextUnderlinePosition(t);
            return;
        }

        unsigned t = 0;
        for (CSSValueListIterator i(value); i.hasMore(); i.advance()) {
            CSSValue* item = i.value();
            TextUnderlinePosition t2 = *toCSSPrimitiveValue(item);
            t |= t2;
        }
        state.style()->setTextUnderlinePosition(static_cast<TextUnderlinePosition>(t));
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<TextUnderlinePosition, &RenderStyle::textUnderlinePosition, TextUnderlinePosition, &RenderStyle::setTextUnderlinePosition, TextUnderlinePosition, &RenderStyle::initialTextUnderlinePosition>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};
#endif // CSS3_TEXT

class ApplyPropertyLineHeight {
public:
    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        Length lineHeight;

        if (primitiveValue->getValueID() == CSSValueNormal)
            lineHeight = RenderStyle::initialLineHeight();
        else if (primitiveValue->isLength()) {
            double multiplier = state.style()->effectiveZoom();
            if (Frame* frame = styleResolver->document()->frame())
                multiplier *= frame->textZoomFactor();
            lineHeight = primitiveValue->computeLength<Length>(state.style(), state.rootElementStyle(), multiplier);
        } else if (primitiveValue->isPercentage()) {
            // FIXME: percentage should not be restricted to an integer here.
            lineHeight = Length((state.style()->fontSize() * primitiveValue->getIntValue()) / 100, Fixed);
        } else if (primitiveValue->isNumber()) {
            // FIXME: number and percentage values should produce the same type of Length (ie. Fixed or Percent).
            lineHeight = Length(primitiveValue->getDoubleValue() * 100.0, Percent);
        } else if (primitiveValue->isViewportPercentageLength())
            lineHeight = primitiveValue->viewportPercentageLength();
        else
            return;
        state.style()->setLineHeight(lineHeight);
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<Length, &RenderStyle::specifiedLineHeight, Length, &RenderStyle::setLineHeight, Length, &RenderStyle::initialLineHeight>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

class ApplyPropertyPageSize {
private:
    static Length mmLength(double mm) { return CSSPrimitiveValue::create(mm, CSSPrimitiveValue::CSS_MM)->computeLength<Length>(0, 0); }
    static Length inchLength(double inch) { return CSSPrimitiveValue::create(inch, CSSPrimitiveValue::CSS_IN)->computeLength<Length>(0, 0); }
    static bool getPageSizeFromName(CSSPrimitiveValue* pageSizeName, CSSPrimitiveValue* pageOrientation, Length& width, Length& height)
    {
        DEFINE_STATIC_LOCAL(Length, a5Width, (mmLength(148)));
        DEFINE_STATIC_LOCAL(Length, a5Height, (mmLength(210)));
        DEFINE_STATIC_LOCAL(Length, a4Width, (mmLength(210)));
        DEFINE_STATIC_LOCAL(Length, a4Height, (mmLength(297)));
        DEFINE_STATIC_LOCAL(Length, a3Width, (mmLength(297)));
        DEFINE_STATIC_LOCAL(Length, a3Height, (mmLength(420)));
        DEFINE_STATIC_LOCAL(Length, b5Width, (mmLength(176)));
        DEFINE_STATIC_LOCAL(Length, b5Height, (mmLength(250)));
        DEFINE_STATIC_LOCAL(Length, b4Width, (mmLength(250)));
        DEFINE_STATIC_LOCAL(Length, b4Height, (mmLength(353)));
        DEFINE_STATIC_LOCAL(Length, letterWidth, (inchLength(8.5)));
        DEFINE_STATIC_LOCAL(Length, letterHeight, (inchLength(11)));
        DEFINE_STATIC_LOCAL(Length, legalWidth, (inchLength(8.5)));
        DEFINE_STATIC_LOCAL(Length, legalHeight, (inchLength(14)));
        DEFINE_STATIC_LOCAL(Length, ledgerWidth, (inchLength(11)));
        DEFINE_STATIC_LOCAL(Length, ledgerHeight, (inchLength(17)));

        if (!pageSizeName)
            return false;

        switch (pageSizeName->getValueID()) {
        case CSSValueA5:
            width = a5Width;
            height = a5Height;
            break;
        case CSSValueA4:
            width = a4Width;
            height = a4Height;
            break;
        case CSSValueA3:
            width = a3Width;
            height = a3Height;
            break;
        case CSSValueB5:
            width = b5Width;
            height = b5Height;
            break;
        case CSSValueB4:
            width = b4Width;
            height = b4Height;
            break;
        case CSSValueLetter:
            width = letterWidth;
            height = letterHeight;
            break;
        case CSSValueLegal:
            width = legalWidth;
            height = legalHeight;
            break;
        case CSSValueLedger:
            width = ledgerWidth;
            height = ledgerHeight;
            break;
        default:
            return false;
        }

        if (pageOrientation) {
            switch (pageOrientation->getValueID()) {
            case CSSValueLandscape:
                std::swap(width, height);
                break;
            case CSSValuePortrait:
                // Nothing to do.
                break;
            default:
                return false;
            }
        }
        return true;
    }
public:
    static void applyInheritValue(CSSPropertyID, StyleResolver*, StyleResolverState&) { }
    static void applyInitialValue(CSSPropertyID, StyleResolver*, StyleResolverState&) { }
    static void applyValue(CSSPropertyID, StyleResolver*, StyleResolverState& state, CSSValue* value)
    {
        state.style()->resetPageSizeType();
        Length width;
        Length height;
        PageSizeType pageSizeType = PAGE_SIZE_AUTO;
        CSSValueListInspector inspector(value);
        switch (inspector.length()) {
        case 2: {
            // <length>{2} | <page-size> <orientation>
            if (!inspector.first()->isPrimitiveValue() || !inspector.second()->isPrimitiveValue())
                return;
            CSSPrimitiveValue* first = toCSSPrimitiveValue(inspector.first());
            CSSPrimitiveValue* second = toCSSPrimitiveValue(inspector.second());
            if (first->isLength()) {
                // <length>{2}
                if (!second->isLength())
                    return;
                width = first->computeLength<Length>(state.style(), state.rootElementStyle());
                height = second->computeLength<Length>(state.style(), state.rootElementStyle());
            } else {
                // <page-size> <orientation>
                // The value order is guaranteed. See CSSParser::parseSizeParameter.
                if (!getPageSizeFromName(first, second, width, height))
                    return;
            }
            pageSizeType = PAGE_SIZE_RESOLVED;
            break;
        }
        case 1: {
            // <length> | auto | <page-size> | [ portrait | landscape]
            if (!inspector.first()->isPrimitiveValue())
                return;
            CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(inspector.first());
            if (primitiveValue->isLength()) {
                // <length>
                pageSizeType = PAGE_SIZE_RESOLVED;
                width = height = primitiveValue->computeLength<Length>(state.style(), state.rootElementStyle());
            } else {
                switch (primitiveValue->getValueID()) {
                case 0:
                    return;
                case CSSValueAuto:
                    pageSizeType = PAGE_SIZE_AUTO;
                    break;
                case CSSValuePortrait:
                    pageSizeType = PAGE_SIZE_AUTO_PORTRAIT;
                    break;
                case CSSValueLandscape:
                    pageSizeType = PAGE_SIZE_AUTO_LANDSCAPE;
                    break;
                default:
                    // <page-size>
                    pageSizeType = PAGE_SIZE_RESOLVED;
                    if (!getPageSizeFromName(primitiveValue, 0, width, height))
                        return;
                }
            }
            break;
        }
        default:
            return;
        }
        state.style()->setPageSizeType(pageSizeType);
        state.style()->setPageSize(LengthSize(width, height));
    }
    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

class ApplyPropertyTextEmphasisStyle {
public:
    static void applyInheritValue(CSSPropertyID, StyleResolver*, StyleResolverState& state)
    {
        state.style()->setTextEmphasisFill(state.parentStyle()->textEmphasisFill());
        state.style()->setTextEmphasisMark(state.parentStyle()->textEmphasisMark());
        state.style()->setTextEmphasisCustomMark(state.parentStyle()->textEmphasisCustomMark());
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver*, StyleResolverState& state)
    {
        state.style()->setTextEmphasisFill(RenderStyle::initialTextEmphasisFill());
        state.style()->setTextEmphasisMark(RenderStyle::initialTextEmphasisMark());
        state.style()->setTextEmphasisCustomMark(RenderStyle::initialTextEmphasisCustomMark());
    }

    static void applyValue(CSSPropertyID, StyleResolver*, StyleResolverState& state, CSSValue* value)
    {
        if (value->isValueList()) {
            CSSValueList* list = toCSSValueList(value);
            ASSERT(list->length() == 2);
            if (list->length() != 2)
                return;
            for (unsigned i = 0; i < 2; ++i) {
                CSSValue* item = list->itemWithoutBoundsCheck(i);
                if (!item->isPrimitiveValue())
                    continue;

                CSSPrimitiveValue* value = toCSSPrimitiveValue(item);
                if (value->getValueID() == CSSValueFilled || value->getValueID() == CSSValueOpen)
                    state.style()->setTextEmphasisFill(*value);
                else
                    state.style()->setTextEmphasisMark(*value);
            }
            state.style()->setTextEmphasisCustomMark(nullAtom);
            return;
        }

        if (!value->isPrimitiveValue())
            return;
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

        if (primitiveValue->isString()) {
            state.style()->setTextEmphasisFill(TextEmphasisFillFilled);
            state.style()->setTextEmphasisMark(TextEmphasisMarkCustom);
            state.style()->setTextEmphasisCustomMark(primitiveValue->getStringValue());
            return;
        }

        state.style()->setTextEmphasisCustomMark(nullAtom);

        if (primitiveValue->getValueID() == CSSValueFilled || primitiveValue->getValueID() == CSSValueOpen) {
            state.style()->setTextEmphasisFill(*primitiveValue);
            state.style()->setTextEmphasisMark(TextEmphasisMarkAuto);
        } else {
            state.style()->setTextEmphasisFill(TextEmphasisFillFilled);
            state.style()->setTextEmphasisMark(*primitiveValue);
        }
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

class ApplyPropertyOutlineStyle {
public:
    static void applyInheritValue(CSSPropertyID propertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        ApplyPropertyDefaultBase<OutlineIsAuto, &RenderStyle::outlineStyleIsAuto, OutlineIsAuto, &RenderStyle::setOutlineStyleIsAuto, OutlineIsAuto, &RenderStyle::initialOutlineStyleIsAuto>::applyInheritValue(propertyID, styleResolver, state);
        ApplyPropertyDefaultBase<EBorderStyle, &RenderStyle::outlineStyle, EBorderStyle, &RenderStyle::setOutlineStyle, EBorderStyle, &RenderStyle::initialBorderStyle>::applyInheritValue(propertyID, styleResolver, state);
    }

    static void applyInitialValue(CSSPropertyID propertyID, StyleResolver* styleResolver, StyleResolverState& state)
    {
        ApplyPropertyDefaultBase<OutlineIsAuto, &RenderStyle::outlineStyleIsAuto, OutlineIsAuto, &RenderStyle::setOutlineStyleIsAuto, OutlineIsAuto, &RenderStyle::initialOutlineStyleIsAuto>::applyInitialValue(propertyID, styleResolver, state);
        ApplyPropertyDefaultBase<EBorderStyle, &RenderStyle::outlineStyle, EBorderStyle, &RenderStyle::setOutlineStyle, EBorderStyle, &RenderStyle::initialBorderStyle>::applyInitialValue(propertyID, styleResolver, state);
    }

    static void applyValue(CSSPropertyID propertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        ApplyPropertyDefault<OutlineIsAuto, &RenderStyle::outlineStyleIsAuto, OutlineIsAuto, &RenderStyle::setOutlineStyleIsAuto, OutlineIsAuto, &RenderStyle::initialOutlineStyleIsAuto>::applyValue(propertyID, styleResolver, state, value);
        ApplyPropertyDefault<EBorderStyle, &RenderStyle::outlineStyle, EBorderStyle, &RenderStyle::setOutlineStyle, EBorderStyle, &RenderStyle::initialBorderStyle>::applyValue(propertyID, styleResolver, state, value);
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

class ApplyPropertyResize {
public:
    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

        EResize r = RESIZE_NONE;
        switch (primitiveValue->getValueID()) {
        case 0:
            return;
        case CSSValueAuto:
            if (Settings* settings = styleResolver->document()->settings())
                r = settings->textAreasAreResizable() ? RESIZE_BOTH : RESIZE_NONE;
            break;
        default:
            r = *primitiveValue;
        }
        state.style()->setResize(r);
    }

    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<EResize, &RenderStyle::resize, EResize, &RenderStyle::setResize, EResize, &RenderStyle::initialResize>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

class ApplyPropertyVerticalAlign {
public:
    static void applyValue(CSSPropertyID, StyleResolver*, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

        if (primitiveValue->getValueID())
            return state.style()->setVerticalAlign(*primitiveValue);

        state.style()->setVerticalAlignLength(primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion | ViewportPercentageConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom()));
    }

    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<EVerticalAlign, &RenderStyle::verticalAlign, EVerticalAlign, &RenderStyle::setVerticalAlign, EVerticalAlign, &RenderStyle::initialVerticalAlign>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

class ApplyPropertyAspectRatio {
public:
    static void applyInheritValue(CSSPropertyID, StyleResolver*, StyleResolverState& state)
    {
        if (!state.parentStyle()->hasAspectRatio())
            return;
        state.style()->setHasAspectRatio(true);
        state.style()->setAspectRatioDenominator(state.parentStyle()->aspectRatioDenominator());
        state.style()->setAspectRatioNumerator(state.parentStyle()->aspectRatioNumerator());
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver*, StyleResolverState& state)
    {
        state.style()->setHasAspectRatio(RenderStyle::initialHasAspectRatio());
        state.style()->setAspectRatioDenominator(RenderStyle::initialAspectRatioDenominator());
        state.style()->setAspectRatioNumerator(RenderStyle::initialAspectRatioNumerator());
    }

    static void applyValue(CSSPropertyID, StyleResolver*, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isAspectRatioValue()) {
            state.style()->setHasAspectRatio(false);
            return;
        }
        CSSAspectRatioValue* aspectRatioValue = static_cast<CSSAspectRatioValue*>(value);
        state.style()->setHasAspectRatio(true);
        state.style()->setAspectRatioDenominator(aspectRatioValue->denominatorValue());
        state.style()->setAspectRatioNumerator(aspectRatioValue->numeratorValue());
    }

    static PropertyHandler createHandler()
    {
        return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue);
    }
};

class ApplyPropertyDisplay {
private:
    static inline bool isValidDisplayValue(StyleResolverState& state, EDisplay displayPropertyValue)
    {
        if (state.element() && state.element()->isSVGElement() && state.style()->styleType() == NOPSEUDO)
            return (displayPropertyValue == INLINE || displayPropertyValue == BLOCK || displayPropertyValue == NONE);
        return true;
    }
public:
    static void applyInheritValue(CSSPropertyID, StyleResolver*, StyleResolverState& state)
    {
        EDisplay display = state.parentStyle()->display();
        if (!isValidDisplayValue(state, display))
            return;
        state.style()->setDisplay(display);
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver*, StyleResolverState& state)
    {
        state.style()->setDisplay(RenderStyle::initialDisplay());
    }

    static void applyValue(CSSPropertyID, StyleResolver*, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        EDisplay display = *toCSSPrimitiveValue(value);

        if (!isValidDisplayValue(state, display))
            return;

        state.style()->setDisplay(display);
    }

    static PropertyHandler createHandler()
    {
        return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue);
    }
};

template <ClipPathOperation* (RenderStyle::*getterFunction)() const, void (RenderStyle::*setterFunction)(PassRefPtr<ClipPathOperation>), ClipPathOperation* (*initialFunction)()>
class ApplyPropertyClipPath {
public:
    static void setValue(RenderStyle* style, PassRefPtr<ClipPathOperation> value) { (style->*setterFunction)(value); }
    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        if (value->isPrimitiveValue()) {
            CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
            if (primitiveValue->getValueID() == CSSValueNone)
                setValue(state.style(), 0);
            else if (primitiveValue->isShape()) {
                setValue(state.style(), ShapeClipPathOperation::create(basicShapeForValue(state, primitiveValue->getShapeValue())));
            } else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_URI) {
                String cssURLValue = primitiveValue->getStringValue();
                KURL url = styleResolver->document()->completeURL(cssURLValue);
                // FIXME: It doesn't work with forward or external SVG references (see https://bugs.webkit.org/show_bug.cgi?id=90405)
                setValue(state.style(), ReferenceClipPathOperation::create(cssURLValue, url.fragmentIdentifier()));
            }
        }
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<ClipPathOperation*, getterFunction, PassRefPtr<ClipPathOperation>, setterFunction, ClipPathOperation*, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

template <ShapeValue* (RenderStyle::*getterFunction)() const, void (RenderStyle::*setterFunction)(PassRefPtr<ShapeValue>), ShapeValue* (*initialFunction)()>
class ApplyPropertyShape {
public:
    static void setValue(RenderStyle* style, PassRefPtr<ShapeValue> value) { (style->*setterFunction)(value); }
    static void applyValue(CSSPropertyID property, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
    {
        if (value->isPrimitiveValue()) {
            CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
            if (primitiveValue->getValueID() == CSSValueAuto)
                setValue(state.style(), 0);
            // FIXME Bug 102571: Layout for the value 'outside-shape' is not yet implemented
            else if (primitiveValue->getValueID() == CSSValueOutsideShape)
                setValue(state.style(), ShapeValue::createOutsideValue());
            else if (primitiveValue->isShape()) {
                setValue(state.style(), ShapeValue::createShapeValue(basicShapeForValue(state, primitiveValue->getShapeValue())));
            }
        } else if (value->isImageValue()) {
            setValue(state.style(), ShapeValue::createImageValue(state.styleImage(property, value)));
        }
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<ShapeValue*, getterFunction, PassRefPtr<ShapeValue>, setterFunction, ShapeValue*, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

class ApplyPropertyTextIndent {
public:
    static void applyInheritValue(CSSPropertyID, StyleResolver*, StyleResolverState& state)
    {
        state.style()->setTextIndent(state.parentStyle()->textIndent());
#if ENABLE(CSS3_TEXT)
        state.style()->setTextIndentLine(state.parentStyle()->textIndentLine());
#endif
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver*, StyleResolverState& state)
    {
        state.style()->setTextIndent(RenderStyle::initialTextIndent());
#if ENABLE(CSS3_TEXT)
        state.style()->setTextIndentLine(RenderStyle::initialTextIndentLine());
#endif
    }

    static void applyValue(CSSPropertyID, StyleResolver*, StyleResolverState& state, CSSValue* value)
    {
        if (!value->isValueList())
            return;

        // [ <length> | <percentage> ] -webkit-each-line
        // The order is guaranteed. See CSSParser::parseTextIndent.
        // The second value, -webkit-each-line is handled only when CSS3_TEXT is enabled.

        CSSValueList* valueList = toCSSValueList(value);
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(valueList->itemWithoutBoundsCheck(0));
        Length lengthOrPercentageValue = primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion | ViewportPercentageConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom());
        ASSERT(!lengthOrPercentageValue.isUndefined());
        state.style()->setTextIndent(lengthOrPercentageValue);

#if ENABLE(CSS3_TEXT)
        ASSERT(valueList->length() <= 2);
        CSSPrimitiveValue* eachLineValue = toCSSPrimitiveValue(valueList->item(1));
        if (eachLineValue) {
            ASSERT(eachLineValue->getValueID() == CSSValueWebkitEachLine);
            state.style()->setTextIndentLine(TextIndentEachLine);
        } else {
            state.style()->setTextIndentLine(TextIndentFirstLine);
        }
#endif
    }

    static PropertyHandler createHandler()
    {
        return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue);
    }
};

const DeprecatedStyleBuilder& DeprecatedStyleBuilder::sharedStyleBuilder()
{
    DEFINE_STATIC_LOCAL(DeprecatedStyleBuilder, styleBuilderInstance, ());
    return styleBuilderInstance;
}

DeprecatedStyleBuilder::DeprecatedStyleBuilder()
{
    for (int i = 0; i < numCSSProperties; ++i)
        m_propertyMap[i] = PropertyHandler();

    // Please keep CSS property list in alphabetical order.
    setPropertyHandler(CSSPropertyBorderBottomWidth, ApplyPropertyComputeLength<unsigned, &RenderStyle::borderBottomWidth, &RenderStyle::setBorderBottomWidth, &RenderStyle::initialBorderWidth, NormalDisabled, ThicknessEnabled>::createHandler());
    setPropertyHandler(CSSPropertyBorderLeftWidth, ApplyPropertyComputeLength<unsigned, &RenderStyle::borderLeftWidth, &RenderStyle::setBorderLeftWidth, &RenderStyle::initialBorderWidth, NormalDisabled, ThicknessEnabled>::createHandler());
    setPropertyHandler(CSSPropertyBorderRightWidth, ApplyPropertyComputeLength<unsigned, &RenderStyle::borderRightWidth, &RenderStyle::setBorderRightWidth, &RenderStyle::initialBorderWidth, NormalDisabled, ThicknessEnabled>::createHandler());
    setPropertyHandler(CSSPropertyBorderTopWidth, ApplyPropertyComputeLength<unsigned, &RenderStyle::borderTopWidth, &RenderStyle::setBorderTopWidth, &RenderStyle::initialBorderWidth, NormalDisabled, ThicknessEnabled>::createHandler());
    setPropertyHandler(CSSPropertyClip, ApplyPropertyClip::createHandler());
    setPropertyHandler(CSSPropertyCounterIncrement, ApplyPropertyCounter<Increment>::createHandler());
    setPropertyHandler(CSSPropertyCounterReset, ApplyPropertyCounter<Reset>::createHandler());
    setPropertyHandler(CSSPropertyCursor, ApplyPropertyCursor::createHandler());
    setPropertyHandler(CSSPropertyDirection, ApplyPropertyDirection<&RenderStyle::direction, &RenderStyle::setDirection, RenderStyle::initialDirection>::createHandler());
    setPropertyHandler(CSSPropertyDisplay, ApplyPropertyDisplay::createHandler());
    setPropertyHandler(CSSPropertyFontFamily, ApplyPropertyFontFamily::createHandler());
    setPropertyHandler(CSSPropertyFontSize, ApplyPropertyFontSize::createHandler());
    setPropertyHandler(CSSPropertyFontStyle, ApplyPropertyFont<FontItalic, &FontDescription::italic, &FontDescription::setItalic, FontItalicOff>::createHandler());
    setPropertyHandler(CSSPropertyFontVariant, ApplyPropertyFont<FontSmallCaps, &FontDescription::smallCaps, &FontDescription::setSmallCaps, FontSmallCapsOff>::createHandler());
    setPropertyHandler(CSSPropertyFontWeight, ApplyPropertyFontWeight::createHandler());
    setPropertyHandler(CSSPropertyLetterSpacing, ApplyPropertyComputeLength<float, &RenderStyle::letterSpacing, &RenderStyle::setLetterSpacing, &RenderStyle::initialLetterWordSpacing, NormalEnabled, ThicknessDisabled, SVGZoomEnabled>::createHandler());
    setPropertyHandler(CSSPropertyLineHeight, ApplyPropertyLineHeight::createHandler());
    setPropertyHandler(CSSPropertyListStyleImage, ApplyPropertyStyleImage<&RenderStyle::listStyleImage, &RenderStyle::setListStyleImage, &RenderStyle::initialListStyleImage, CSSPropertyListStyleImage>::createHandler());
    setPropertyHandler(CSSPropertyOutlineOffset, ApplyPropertyComputeLength<int, &RenderStyle::outlineOffset, &RenderStyle::setOutlineOffset, &RenderStyle::initialOutlineOffset>::createHandler());
    setPropertyHandler(CSSPropertyOutlineStyle, ApplyPropertyOutlineStyle::createHandler());
    setPropertyHandler(CSSPropertyOutlineWidth, ApplyPropertyComputeLength<unsigned short, &RenderStyle::outlineWidth, &RenderStyle::setOutlineWidth, &RenderStyle::initialOutlineWidth, NormalDisabled, ThicknessEnabled>::createHandler());
    setPropertyHandler(CSSPropertyResize, ApplyPropertyResize::createHandler());
    setPropertyHandler(CSSPropertySize, ApplyPropertyPageSize::createHandler());
    setPropertyHandler(CSSPropertyTextAlign, ApplyPropertyTextAlign::createHandler());
    setPropertyHandler(CSSPropertyTextDecoration, ApplyPropertyTextDecoration::createHandler());
    setPropertyHandler(CSSPropertyTextDecorationLine, ApplyPropertyTextDecoration::createHandler());

#if ENABLE(CSS3_TEXT)
    setPropertyHandler(CSSPropertyWebkitTextAlignLast, ApplyPropertyDefault<TextAlignLast, &RenderStyle::textAlignLast, TextAlignLast, &RenderStyle::setTextAlignLast, TextAlignLast, &RenderStyle::initialTextAlignLast>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTextUnderlinePosition, ApplyPropertyTextUnderlinePosition::createHandler());
#endif // CSS3_TEXT
    setPropertyHandler(CSSPropertyTextIndent, ApplyPropertyTextIndent::createHandler());
    setPropertyHandler(CSSPropertyTextRendering, ApplyPropertyFont<TextRenderingMode, &FontDescription::textRenderingMode, &FontDescription::setTextRenderingMode, AutoTextRendering>::createHandler());
    setPropertyHandler(CSSPropertyVerticalAlign, ApplyPropertyVerticalAlign::createHandler());
    setPropertyHandler(CSSPropertyWebkitAspectRatio, ApplyPropertyAspectRatio::createHandler());
    setPropertyHandler(CSSPropertyWebkitBorderHorizontalSpacing, ApplyPropertyComputeLength<short, &RenderStyle::horizontalBorderSpacing, &RenderStyle::setHorizontalBorderSpacing, &RenderStyle::initialHorizontalBorderSpacing>::createHandler());
    setPropertyHandler(CSSPropertyWebkitBorderVerticalSpacing, ApplyPropertyComputeLength<short, &RenderStyle::verticalBorderSpacing, &RenderStyle::setVerticalBorderSpacing, &RenderStyle::initialVerticalBorderSpacing>::createHandler());
    setPropertyHandler(CSSPropertyWebkitColumnRuleWidth, ApplyPropertyComputeLength<unsigned short, &RenderStyle::columnRuleWidth, &RenderStyle::setColumnRuleWidth, &RenderStyle::initialColumnRuleWidth, NormalDisabled, ThicknessEnabled>::createHandler());
    setPropertyHandler(CSSPropertyWebkitFontKerning, ApplyPropertyFont<FontDescription::Kerning, &FontDescription::kerning, &FontDescription::setKerning, FontDescription::AutoKerning>::createHandler());
    setPropertyHandler(CSSPropertyWebkitFontSmoothing, ApplyPropertyFont<FontSmoothingMode, &FontDescription::fontSmoothing, &FontDescription::setFontSmoothing, AutoSmoothing>::createHandler());
    setPropertyHandler(CSSPropertyWebkitFontVariantLigatures, ApplyPropertyFontVariantLigatures::createHandler());
    setPropertyHandler(CSSPropertyWebkitMarqueeSpeed, ApplyPropertyMarqueeSpeed::createHandler());
    setPropertyHandler(CSSPropertyWebkitPerspectiveOrigin, ApplyPropertyExpanding<SuppressValue, CSSPropertyWebkitPerspectiveOriginX, CSSPropertyWebkitPerspectiveOriginY>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTextEmphasisStyle, ApplyPropertyTextEmphasisStyle::createHandler());
    setPropertyHandler(CSSPropertyWebkitTransformOriginZ, ApplyPropertyComputeLength<float, &RenderStyle::transformOriginZ, &RenderStyle::setTransformOriginZ, &RenderStyle::initialTransformOriginZ>::createHandler());
    setPropertyHandler(CSSPropertyWebkitClipPath, ApplyPropertyClipPath<&RenderStyle::clipPath, &RenderStyle::setClipPath, &RenderStyle::initialClipPath>::createHandler());
    setPropertyHandler(CSSPropertyWebkitShapeInside, ApplyPropertyShape<&RenderStyle::shapeInside, &RenderStyle::setShapeInside, &RenderStyle::initialShapeInside>::createHandler());
    setPropertyHandler(CSSPropertyWebkitShapeOutside, ApplyPropertyShape<&RenderStyle::shapeOutside, &RenderStyle::setShapeOutside, &RenderStyle::initialShapeOutside>::createHandler());
    setPropertyHandler(CSSPropertyWordSpacing, ApplyPropertyComputeLength<float, &RenderStyle::wordSpacing, &RenderStyle::setWordSpacing, &RenderStyle::initialLetterWordSpacing, NormalEnabled, ThicknessDisabled, SVGZoomEnabled>::createHandler());
}


}
