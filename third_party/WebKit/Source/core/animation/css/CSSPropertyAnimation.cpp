/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/animation/css/CSSPropertyAnimation.h"

#include <algorithm>
#include "StylePropertyShorthand.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/fetch/ImageResource.h"
#include "core/rendering/ClipPathOperation.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/ShadowList.h"
#include "core/rendering/style/StyleFetchedImage.h"
#include "core/rendering/style/StyleGeneratedImage.h"
#include "platform/FloatConversion.h"
#include "wtf/Noncopyable.h"

namespace WebCore {

class AnimationPropertyWrapperBase {
    WTF_MAKE_NONCOPYABLE(AnimationPropertyWrapperBase);
    WTF_MAKE_FAST_ALLOCATED;
public:
    AnimationPropertyWrapperBase(CSSPropertyID prop)
        : m_prop(prop)
    {
    }

    virtual ~AnimationPropertyWrapperBase() { }

    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const = 0;

    CSSPropertyID property() const { return m_prop; }

private:
    CSSPropertyID m_prop;
};

static int gPropertyWrapperMap[numCSSProperties];
static const int cInvalidPropertyWrapperIndex = -1;
static Vector<AnimationPropertyWrapperBase*>* gPropertyWrappers = 0;

static AnimationPropertyWrapperBase* wrapperForProperty(CSSPropertyID propertyID)
{
    int propIndex = propertyID - firstCSSProperty;
    if (propIndex >= 0 && propIndex < numCSSProperties) {
        int wrapperIndex = gPropertyWrapperMap[propIndex];
        if (wrapperIndex >= 0)
            return (*gPropertyWrappers)[wrapperIndex];
    }
    return 0;
}

template <typename T>
class PropertyWrapperGetter : public AnimationPropertyWrapperBase {
public:
    PropertyWrapperGetter(CSSPropertyID prop, T (RenderStyle::*getter)() const)
        : AnimationPropertyWrapperBase(prop)
        , m_getter(getter)
    {
    }

    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const
    {
        // If the style pointers are the same, don't bother doing the test.
        // If either is null, return false. If both are null, return true.
        if ((!a && !b) || a == b)
            return true;
        if (!a || !b)
            return false;
        return (a->*m_getter)() == (b->*m_getter)();
    }

protected:
    T (RenderStyle::*m_getter)() const;
};

template <typename T>
class PropertyWrapper : public PropertyWrapperGetter<T> {
public:
    PropertyWrapper(CSSPropertyID prop, T (RenderStyle::*getter)() const)
        : PropertyWrapperGetter<T>(prop, getter)
    {
    }
};

template <typename T>
class RefCountedPropertyWrapper : public PropertyWrapperGetter<T*> {
public:
    RefCountedPropertyWrapper(CSSPropertyID prop, T* (RenderStyle::*getter)() const)
        : PropertyWrapperGetter<T*>(prop, getter)
    {
    }

    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const OVERRIDE
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;
        const T* aValue = (a->*this->m_getter)();
        const T* bValue = (b->*this->m_getter)();
        if (aValue == bValue)
            return true;
        if (!aValue || !bValue)
            return false;
        return *aValue == *bValue;
    }
};


class StyleImagePropertyWrapper FINAL : public RefCountedPropertyWrapper<StyleImage> {
public:
    StyleImagePropertyWrapper(CSSPropertyID prop, StyleImage* (RenderStyle::*getter)() const)
        : RefCountedPropertyWrapper<StyleImage>(prop, getter)
    {
    }

    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const OVERRIDE
    {
       // If the style pointers are the same, don't bother doing the test.
       // If either is null, return false. If both are null, return true.
       if (a == b)
           return true;
       if (!a || !b)
            return false;

        StyleImage* imageA = (a->*m_getter)();
        StyleImage* imageB = (b->*m_getter)();
        return StyleImage::imagesEquivalent(imageA, imageB);
    }
};

class PropertyWrapperShadow FINAL : public AnimationPropertyWrapperBase {
public:
    PropertyWrapperShadow(CSSPropertyID prop, ShadowList* (RenderStyle::*getter)() const)
        : AnimationPropertyWrapperBase(prop)
        , m_getter(getter)
    {
    }

    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const OVERRIDE
    {
        const ShadowList* shadowA = (a->*m_getter)();
        const ShadowList* shadowB = (b->*m_getter)();
        if (shadowA == shadowB)
            return true;
        if (shadowA && shadowB)
            return *shadowA == *shadowB;
        return false;
    }

    ShadowList* (RenderStyle::*m_getter)() const;
};

class PropertyWrapperMaybeInvalidStyleColor FINAL : public AnimationPropertyWrapperBase {
public:
    PropertyWrapperMaybeInvalidStyleColor(CSSPropertyID prop, StyleColor (RenderStyle::*getter)() const)
        : AnimationPropertyWrapperBase(prop)
        , m_getter(getter)
    {
    }

    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const OVERRIDE
    {
        StyleColor fromColor = (a->*m_getter)();
        StyleColor toColor = (b->*m_getter)();

        if (fromColor.isCurrentColor() && toColor.isCurrentColor())
            return true;

        return fromColor.resolve(a->color()) == toColor.resolve(b->color());
    }

private:
    StyleColor (RenderStyle::*m_getter)() const;
};


class PropertyWrapperVisitedAffectedColor FINAL : public AnimationPropertyWrapperBase {
public:
    PropertyWrapperVisitedAffectedColor(CSSPropertyID prop, Color (RenderStyle::*getter)() const, Color (RenderStyle::*visitedGetter)() const)
        : AnimationPropertyWrapperBase(prop)
        , m_wrapper(adoptPtr(new PropertyWrapper<Color>(prop, getter)))
        , m_visitedWrapper(adoptPtr(new PropertyWrapper<Color>(prop, visitedGetter)))
    {
    }
    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const
    {
        return m_wrapper->equals(a, b) && m_visitedWrapper->equals(a, b);
    }

private:
    OwnPtr<AnimationPropertyWrapperBase> m_wrapper;
    OwnPtr<AnimationPropertyWrapperBase> m_visitedWrapper;
};

class PropertyWrapperVisitedAffectedStyleColor FINAL : public AnimationPropertyWrapperBase {
public:
    PropertyWrapperVisitedAffectedStyleColor(CSSPropertyID prop, StyleColor (RenderStyle::*getter)() const, StyleColor (RenderStyle::*visitedGetter)() const)
        : AnimationPropertyWrapperBase(prop)
        , m_wrapper(adoptPtr(new PropertyWrapperMaybeInvalidStyleColor(prop, getter)))
        , m_visitedWrapper(adoptPtr(new PropertyWrapperMaybeInvalidStyleColor(prop, visitedGetter)))
    {
    }
    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const OVERRIDE
    {
        return m_wrapper->equals(a, b) && m_visitedWrapper->equals(a, b);
    }

private:
    OwnPtr<AnimationPropertyWrapperBase> m_wrapper;
    OwnPtr<AnimationPropertyWrapperBase> m_visitedWrapper;
};

// Wrapper base class for an animatable property in a FillLayer
class FillLayerAnimationPropertyWrapperBase {
public:
    FillLayerAnimationPropertyWrapperBase()
    {
    }

    virtual ~FillLayerAnimationPropertyWrapperBase() { }

    virtual bool equals(const FillLayer*, const FillLayer*) const = 0;
};

template <typename T>
class FillLayerPropertyWrapperGetter : public FillLayerAnimationPropertyWrapperBase {
    WTF_MAKE_NONCOPYABLE(FillLayerPropertyWrapperGetter);
public:
    FillLayerPropertyWrapperGetter(T (FillLayer::*getter)() const)
        : m_getter(getter)
    {
    }

    virtual bool equals(const FillLayer* a, const FillLayer* b) const
    {
       // If the style pointers are the same, don't bother doing the test.
       // If either is null, return false. If both are null, return true.
       if ((!a && !b) || a == b)
           return true;
       if (!a || !b)
            return false;
        return (a->*m_getter)() == (b->*m_getter)();
    }

protected:
    T (FillLayer::*m_getter)() const;
};

template <typename T>
class FillLayerPropertyWrapper FINAL : public FillLayerPropertyWrapperGetter<T> {
public:
    FillLayerPropertyWrapper(T (FillLayer::*getter)() const)
        : FillLayerPropertyWrapperGetter<T>(getter)
    {
    }
};

template <typename T>
class FillLayerRefCountedPropertyWrapper : public FillLayerPropertyWrapperGetter<T*> {
public:
    FillLayerRefCountedPropertyWrapper(T* (FillLayer::*getter)() const)
        : FillLayerPropertyWrapperGetter<T*>(getter)
    {
    }
};

class FillLayerStyleImagePropertyWrapper FINAL : public FillLayerRefCountedPropertyWrapper<StyleImage> {
public:
    FillLayerStyleImagePropertyWrapper(StyleImage* (FillLayer::*getter)() const)
        : FillLayerRefCountedPropertyWrapper<StyleImage>(getter)
    {
    }

    virtual bool equals(const FillLayer* a, const FillLayer* b) const OVERRIDE
    {
       // If the style pointers are the same, don't bother doing the test.
       // If either is null, return false. If both are null, return true.
       if (a == b)
           return true;
       if (!a || !b)
            return false;

        StyleImage* imageA = (a->*m_getter)();
        StyleImage* imageB = (b->*m_getter)();
        return StyleImage::imagesEquivalent(imageA, imageB);
    }
};


class FillLayersPropertyWrapper FINAL : public AnimationPropertyWrapperBase {
public:
    typedef const FillLayer* (RenderStyle::*LayersGetter)() const;

    FillLayersPropertyWrapper(CSSPropertyID prop, LayersGetter getter)
        : AnimationPropertyWrapperBase(prop)
        , m_layersGetter(getter)
    {
        switch (prop) {
        case CSSPropertyBackgroundPositionX:
        case CSSPropertyWebkitMaskPositionX:
            m_fillLayerPropertyWrapper = new FillLayerPropertyWrapper<Length>(&FillLayer::xPosition);
            break;
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionY:
            m_fillLayerPropertyWrapper = new FillLayerPropertyWrapper<Length>(&FillLayer::yPosition);
            break;
        case CSSPropertyBackgroundSize:
        case CSSPropertyWebkitBackgroundSize:
        case CSSPropertyWebkitMaskSize:
            m_fillLayerPropertyWrapper = new FillLayerPropertyWrapper<LengthSize>(&FillLayer::sizeLength);
            break;
        case CSSPropertyBackgroundImage:
            m_fillLayerPropertyWrapper = new FillLayerStyleImagePropertyWrapper(&FillLayer::image);
            break;
        default:
            break;
        }
    }

    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const OVERRIDE
    {
        const FillLayer* fromLayer = (a->*m_layersGetter)();
        const FillLayer* toLayer = (b->*m_layersGetter)();

        while (fromLayer && toLayer) {
            if (!m_fillLayerPropertyWrapper->equals(fromLayer, toLayer))
                return false;

            fromLayer = fromLayer->next();
            toLayer = toLayer->next();
        }

        return true;
    }

private:
    FillLayerAnimationPropertyWrapperBase* m_fillLayerPropertyWrapper;

    LayersGetter m_layersGetter;
};

class PropertyWrapperSVGPaint FINAL : public AnimationPropertyWrapperBase {
public:
    PropertyWrapperSVGPaint(CSSPropertyID prop, const SVGPaint::SVGPaintType& (RenderStyle::*paintTypeGetter)() const, Color (RenderStyle::*getter)() const)
        : AnimationPropertyWrapperBase(prop)
        , m_paintTypeGetter(paintTypeGetter)
        , m_getter(getter)
    {
    }

    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const OVERRIDE
    {
        if ((a->*m_paintTypeGetter)() != (b->*m_paintTypeGetter)())
            return false;

        // We only support animations between SVGPaints that are pure Color values.
        // For everything else we must return true for this method, otherwise
        // we will try to animate between values forever.
        if ((a->*m_paintTypeGetter)() == SVGPaint::SVG_PAINTTYPE_RGBCOLOR) {
            Color fromColor = (a->*m_getter)();
            Color toColor = (b->*m_getter)();
            return fromColor == toColor;
        }
        return true;
    }

private:
    const SVGPaint::SVGPaintType& (RenderStyle::*m_paintTypeGetter)() const;
    Color (RenderStyle::*m_getter)() const;
};

template <typename T>
class RefCountedSVGPropertyWrapper : public AnimationPropertyWrapperBase {
public:
    RefCountedSVGPropertyWrapper(CSSPropertyID prop, PassRefPtr<T> (RenderStyle::*getter)() const)
        : AnimationPropertyWrapperBase(prop)
        , m_getter(getter)
    {
    }

    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const OVERRIDE
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;
        RefPtr<T> aValue = (a->*this->m_getter)();
        RefPtr<T> bValue = (b->*this->m_getter)();
        if (aValue == bValue)
            return true;
        if (!aValue || !bValue)
            return false;
        return *aValue == *bValue;
    }

protected:
    PassRefPtr<T> (RenderStyle::*m_getter)() const;
};

void CSSPropertyAnimation::ensurePropertyMap()
{
    // FIXME: This data is never destroyed. Maybe we should ref count it and toss it when the last AnimationController is destroyed?
    if (gPropertyWrappers)
        return;

    gPropertyWrappers = new Vector<AnimationPropertyWrapperBase*>();

    // build the list of property wrappers to do the comparisons and blends
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyLeft, &RenderStyle::left));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyRight, &RenderStyle::right));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyTop, &RenderStyle::top));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyBottom, &RenderStyle::bottom));

    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyWidth, &RenderStyle::width));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMinWidth, &RenderStyle::minWidth));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMaxWidth, &RenderStyle::maxWidth));

    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyHeight, &RenderStyle::height));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMinHeight, &RenderStyle::minHeight));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMaxHeight, &RenderStyle::maxHeight));

    gPropertyWrappers->append(new PropertyWrapper<unsigned>(CSSPropertyBorderLeftWidth, &RenderStyle::borderLeftWidth));
    gPropertyWrappers->append(new PropertyWrapper<unsigned>(CSSPropertyBorderRightWidth, &RenderStyle::borderRightWidth));
    gPropertyWrappers->append(new PropertyWrapper<unsigned>(CSSPropertyBorderTopWidth, &RenderStyle::borderTopWidth));
    gPropertyWrappers->append(new PropertyWrapper<unsigned>(CSSPropertyBorderBottomWidth, &RenderStyle::borderBottomWidth));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMarginLeft, &RenderStyle::marginLeft));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMarginRight, &RenderStyle::marginRight));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMarginTop, &RenderStyle::marginTop));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMarginBottom, &RenderStyle::marginBottom));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyPaddingLeft, &RenderStyle::paddingLeft));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyPaddingRight, &RenderStyle::paddingRight));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyPaddingTop, &RenderStyle::paddingTop));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyPaddingBottom, &RenderStyle::paddingBottom));
    gPropertyWrappers->append(new PropertyWrapperVisitedAffectedColor(CSSPropertyColor, &RenderStyle::color, &RenderStyle::visitedLinkColor));

    gPropertyWrappers->append(new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyBackgroundColor, &RenderStyle::backgroundColor, &RenderStyle::visitedLinkBackgroundColor));

    gPropertyWrappers->append(new FillLayersPropertyWrapper(CSSPropertyBackgroundImage, &RenderStyle::backgroundLayers));
    gPropertyWrappers->append(new StyleImagePropertyWrapper(CSSPropertyListStyleImage, &RenderStyle::listStyleImage));
    gPropertyWrappers->append(new StyleImagePropertyWrapper(CSSPropertyWebkitMaskImage, &RenderStyle::maskImage));

    gPropertyWrappers->append(new StyleImagePropertyWrapper(CSSPropertyBorderImageSource, &RenderStyle::borderImageSource));
    gPropertyWrappers->append(new PropertyWrapper<LengthBox>(CSSPropertyBorderImageSlice, &RenderStyle::borderImageSlices));
    gPropertyWrappers->append(new PropertyWrapper<const BorderImageLengthBox&>(CSSPropertyBorderImageWidth, &RenderStyle::borderImageWidth));
    gPropertyWrappers->append(new PropertyWrapper<const BorderImageLengthBox&>(CSSPropertyBorderImageOutset, &RenderStyle::borderImageOutset));

    gPropertyWrappers->append(new StyleImagePropertyWrapper(CSSPropertyWebkitMaskBoxImageSource, &RenderStyle::maskBoxImageSource));
    gPropertyWrappers->append(new PropertyWrapper<LengthBox>(CSSPropertyWebkitMaskBoxImageSlice, &RenderStyle::maskBoxImageSlices));
    gPropertyWrappers->append(new PropertyWrapper<const BorderImageLengthBox&>(CSSPropertyWebkitMaskBoxImageWidth, &RenderStyle::maskBoxImageWidth));
    gPropertyWrappers->append(new PropertyWrapper<const BorderImageLengthBox&>(CSSPropertyWebkitMaskBoxImageOutset, &RenderStyle::maskBoxImageOutset));

    gPropertyWrappers->append(new FillLayersPropertyWrapper(CSSPropertyBackgroundPositionX, &RenderStyle::backgroundLayers));
    gPropertyWrappers->append(new FillLayersPropertyWrapper(CSSPropertyBackgroundPositionY, &RenderStyle::backgroundLayers));
    gPropertyWrappers->append(new FillLayersPropertyWrapper(CSSPropertyBackgroundSize, &RenderStyle::backgroundLayers));
    gPropertyWrappers->append(new FillLayersPropertyWrapper(CSSPropertyWebkitBackgroundSize, &RenderStyle::backgroundLayers));

    gPropertyWrappers->append(new FillLayersPropertyWrapper(CSSPropertyWebkitMaskPositionX, &RenderStyle::maskLayers));
    gPropertyWrappers->append(new FillLayersPropertyWrapper(CSSPropertyWebkitMaskPositionY, &RenderStyle::maskLayers));
    gPropertyWrappers->append(new FillLayersPropertyWrapper(CSSPropertyWebkitMaskSize, &RenderStyle::maskLayers));

    gPropertyWrappers->append(new PropertyWrapper<LengthPoint>(CSSPropertyObjectPosition, &RenderStyle::objectPosition));

    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyFontSize,
        // Must pass a specified size to setFontSize if Text Autosizing is enabled, but a computed size
        // if text zoom is enabled (if neither is enabled it's irrelevant as they're probably the same).
        // FIXME: Should we introduce an option to pass the computed font size here, allowing consumers to
        // enable text zoom rather than Text Autosizing? See http://crbug.com/227545.
        &RenderStyle::specifiedFontSize));
    gPropertyWrappers->append(new PropertyWrapper<FontWeight>(CSSPropertyFontWeight, &RenderStyle::fontWeight));
    gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyWebkitColumnRuleWidth, &RenderStyle::columnRuleWidth));
    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyWebkitColumnGap, &RenderStyle::columnGap));
    gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyWebkitColumnCount, &RenderStyle::columnCount));
    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyWebkitColumnWidth, &RenderStyle::columnWidth));
    gPropertyWrappers->append(new PropertyWrapper<short>(CSSPropertyWebkitBorderHorizontalSpacing, &RenderStyle::horizontalBorderSpacing));
    gPropertyWrappers->append(new PropertyWrapper<short>(CSSPropertyWebkitBorderVerticalSpacing, &RenderStyle::verticalBorderSpacing));
    gPropertyWrappers->append(new PropertyWrapper<int>(CSSPropertyZIndex, &RenderStyle::zIndex));
    gPropertyWrappers->append(new PropertyWrapper<short>(CSSPropertyOrphans, &RenderStyle::orphans));
    gPropertyWrappers->append(new PropertyWrapper<short>(CSSPropertyWidows, &RenderStyle::widows));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyLineHeight, &RenderStyle::specifiedLineHeight));
    gPropertyWrappers->append(new PropertyWrapper<int>(CSSPropertyOutlineOffset, &RenderStyle::outlineOffset));
    gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyOutlineWidth, &RenderStyle::outlineWidth));
    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyLetterSpacing, &RenderStyle::letterSpacing));
    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyWordSpacing, &RenderStyle::wordSpacing));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyTextIndent, &RenderStyle::textIndent));

    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyWebkitPerspective, &RenderStyle::perspective));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyWebkitPerspectiveOriginX, &RenderStyle::perspectiveOriginX));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyWebkitPerspectiveOriginY, &RenderStyle::perspectiveOriginY));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyWebkitTransformOriginX, &RenderStyle::transformOriginX));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyWebkitTransformOriginY, &RenderStyle::transformOriginY));
    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyWebkitTransformOriginZ, &RenderStyle::transformOriginZ));
    gPropertyWrappers->append(new PropertyWrapper<LengthSize>(CSSPropertyBorderTopLeftRadius, &RenderStyle::borderTopLeftRadius));
    gPropertyWrappers->append(new PropertyWrapper<LengthSize>(CSSPropertyBorderTopRightRadius, &RenderStyle::borderTopRightRadius));
    gPropertyWrappers->append(new PropertyWrapper<LengthSize>(CSSPropertyBorderBottomLeftRadius, &RenderStyle::borderBottomLeftRadius));
    gPropertyWrappers->append(new PropertyWrapper<LengthSize>(CSSPropertyBorderBottomRightRadius, &RenderStyle::borderBottomRightRadius));
    gPropertyWrappers->append(new PropertyWrapper<EVisibility>(CSSPropertyVisibility, &RenderStyle::visibility));
    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyZoom, &RenderStyle::zoom));

    gPropertyWrappers->append(new PropertyWrapper<LengthBox>(CSSPropertyClip, &RenderStyle::clip));

    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyOpacity, &RenderStyle::opacity));
    gPropertyWrappers->append(new PropertyWrapper<const TransformOperations&>(CSSPropertyWebkitTransform, &RenderStyle::transform));
    gPropertyWrappers->append(new PropertyWrapper<const FilterOperations&>(CSSPropertyWebkitFilter, &RenderStyle::filter));

    gPropertyWrappers->append(new RefCountedPropertyWrapper<ClipPathOperation>(CSSPropertyWebkitClipPath, &RenderStyle::clipPath));

    gPropertyWrappers->append(new RefCountedPropertyWrapper<ShapeValue>(CSSPropertyShapeInside, &RenderStyle::shapeInside));
    gPropertyWrappers->append(new RefCountedPropertyWrapper<ShapeValue>(CSSPropertyShapeOutside, &RenderStyle::shapeOutside));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyShapeMargin, &RenderStyle::shapeMargin));
    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyShapeImageThreshold, &RenderStyle::shapeImageThreshold));

    gPropertyWrappers->append(new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyWebkitColumnRuleColor, &RenderStyle::columnRuleColor, &RenderStyle::visitedLinkColumnRuleColor));
    gPropertyWrappers->append(new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyWebkitTextStrokeColor, &RenderStyle::textStrokeColor, &RenderStyle::visitedLinkTextStrokeColor));
    gPropertyWrappers->append(new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyBorderLeftColor, &RenderStyle::borderLeftColor, &RenderStyle::visitedLinkBorderLeftColor));
    gPropertyWrappers->append(new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyBorderRightColor, &RenderStyle::borderRightColor, &RenderStyle::visitedLinkBorderRightColor));
    gPropertyWrappers->append(new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyBorderTopColor, &RenderStyle::borderTopColor, &RenderStyle::visitedLinkBorderTopColor));
    gPropertyWrappers->append(new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyBorderBottomColor, &RenderStyle::borderBottomColor, &RenderStyle::visitedLinkBorderBottomColor));
    gPropertyWrappers->append(new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyOutlineColor, &RenderStyle::outlineColor, &RenderStyle::visitedLinkOutlineColor));

    gPropertyWrappers->append(new PropertyWrapperShadow(CSSPropertyBoxShadow, &RenderStyle::boxShadow));
    gPropertyWrappers->append(new PropertyWrapperShadow(CSSPropertyWebkitBoxShadow, &RenderStyle::boxShadow));
    gPropertyWrappers->append(new PropertyWrapperShadow(CSSPropertyTextShadow, &RenderStyle::textShadow));

    gPropertyWrappers->append(new PropertyWrapperSVGPaint(CSSPropertyFill, &RenderStyle::fillPaintType, &RenderStyle::fillPaintColor));
    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyFillOpacity, &RenderStyle::fillOpacity));

    gPropertyWrappers->append(new PropertyWrapperSVGPaint(CSSPropertyStroke, &RenderStyle::strokePaintType, &RenderStyle::strokePaintColor));
    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyStrokeOpacity, &RenderStyle::strokeOpacity));
    gPropertyWrappers->append(new RefCountedSVGPropertyWrapper<SVGLength>(CSSPropertyStrokeWidth, &RenderStyle::strokeWidth));
    gPropertyWrappers->append(new RefCountedSVGPropertyWrapper<SVGLengthList>(CSSPropertyStrokeDasharray, &RenderStyle::strokeDashArray));
    gPropertyWrappers->append(new RefCountedSVGPropertyWrapper<SVGLength>(CSSPropertyStrokeDashoffset, &RenderStyle::strokeDashOffset));
    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyStrokeMiterlimit, &RenderStyle::strokeMiterLimit));

    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyFloodOpacity, &RenderStyle::floodOpacity));
    gPropertyWrappers->append(new PropertyWrapper<Color>(CSSPropertyFloodColor, &RenderStyle::floodColor));

    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyStopOpacity, &RenderStyle::stopOpacity));
    gPropertyWrappers->append(new PropertyWrapper<Color>(CSSPropertyStopColor, &RenderStyle::stopColor));

    gPropertyWrappers->append(new PropertyWrapper<Color>(CSSPropertyLightingColor, &RenderStyle::lightingColor));

    gPropertyWrappers->append(new RefCountedSVGPropertyWrapper<SVGLength>(CSSPropertyBaselineShift, &RenderStyle::baselineShiftValue));
    gPropertyWrappers->append(new RefCountedSVGPropertyWrapper<SVGLength>(CSSPropertyKerning, &RenderStyle::kerning));

    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyFlexGrow, &RenderStyle::flexGrow));
    gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyFlexShrink, &RenderStyle::flexShrink));
    gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyFlexBasis, &RenderStyle::flexBasis));

    // TODO:
    //
    //  CSSPropertyVerticalAlign

    // Make sure unused slots have a value
    for (unsigned int i = 0; i < static_cast<unsigned int>(numCSSProperties); ++i)
        gPropertyWrapperMap[i] = cInvalidPropertyWrapperIndex;

    size_t n = gPropertyWrappers->size();
    for (unsigned int i = 0; i < n; ++i) {
        CSSPropertyID property = (*gPropertyWrappers)[i]->property();
        ASSERT_WITH_MESSAGE(CSSAnimations::isAnimatableProperty(property), "%s is not whitelisted for animation", getPropertyNameString(property).utf8().data());
        ASSERT(property - firstCSSProperty < numCSSProperties);
        gPropertyWrapperMap[property - firstCSSProperty] = i;
    }
}

bool CSSPropertyAnimation::propertiesEqual(CSSPropertyID prop, const RenderStyle* a, const RenderStyle* b)
{
    // FIXME: transitions of text-decoration-color are broken
    if (prop == CSSPropertyTextDecorationColor)
        return true;

    ensurePropertyMap();
    return wrapperForProperty(prop)->equals(a, b);
}


}
