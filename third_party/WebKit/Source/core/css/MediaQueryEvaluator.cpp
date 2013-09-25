/*
 * CSS Media Query Evaluator
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
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
 */

#include "config.h"
#include "core/css/MediaQueryEvaluator.h"

#include "CSSValueKeywords.h"
#include "core/css/CSSAspectRatioValue.h"
#include "core/css/CSSHelper.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/MediaFeatureNames.h"
#include "core/css/MediaList.h"
#include "core/css/MediaQuery.h"
#include "core/css/MediaQueryExp.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/PlatformScreen.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/style/RenderStyle.h"
#include "wtf/HashMap.h"

namespace WebCore {

using namespace MediaFeatureNames;

enum MediaFeaturePrefix { MinPrefix, MaxPrefix, NoPrefix };

typedef bool (*EvalFunc)(CSSValue*, RenderStyle*, Frame*, MediaFeaturePrefix);
typedef HashMap<StringImpl*, EvalFunc> FunctionMap;
static FunctionMap* gFunctionMap;

MediaQueryEvaluator::MediaQueryEvaluator(bool mediaFeatureResult)
    : m_frame(0)
    , m_style(0)
    , m_expResult(mediaFeatureResult)
{
}

MediaQueryEvaluator::MediaQueryEvaluator(const AtomicString& acceptedMediaType, bool mediaFeatureResult)
    : m_mediaType(acceptedMediaType)
    , m_frame(0)
    , m_style(0)
    , m_expResult(mediaFeatureResult)
{
}

MediaQueryEvaluator::MediaQueryEvaluator(const char* acceptedMediaType, bool mediaFeatureResult)
    : m_mediaType(acceptedMediaType)
    , m_frame(0)
    , m_style(0)
    , m_expResult(mediaFeatureResult)
{
}

MediaQueryEvaluator::MediaQueryEvaluator(const AtomicString& acceptedMediaType, Frame* frame, RenderStyle* style)
    : m_mediaType(acceptedMediaType)
    , m_frame(frame)
    , m_style(style)
    , m_expResult(false) // doesn't matter when we have m_frame and m_style
{
}

MediaQueryEvaluator::~MediaQueryEvaluator()
{
}

bool MediaQueryEvaluator::mediaTypeMatch(const AtomicString& mediaTypeToMatch) const
{
    return mediaTypeToMatch.isEmpty()
        || equalIgnoringCase(mediaTypeToMatch, "all")
        || equalIgnoringCase(mediaTypeToMatch, m_mediaType);
}

bool MediaQueryEvaluator::mediaTypeMatchSpecific(const char* mediaTypeToMatch) const
{
    // Like mediaTypeMatch, but without the special cases for "" and "all".
    ASSERT(mediaTypeToMatch);
    ASSERT(mediaTypeToMatch[0] != '\0');
    ASSERT(!equalIgnoringCase(mediaTypeToMatch, AtomicString("all")));
    return equalIgnoringCase(mediaTypeToMatch, m_mediaType);
}

static bool applyRestrictor(MediaQuery::Restrictor r, bool value)
{
    return r == MediaQuery::Not ? !value : value;
}

bool MediaQueryEvaluator::eval(const MediaQuerySet* querySet, StyleResolver* styleResolver) const
{
    if (!querySet)
        return true;

    const Vector<OwnPtr<MediaQuery> >& queries = querySet->queryVector();
    if (!queries.size())
        return true; // empty query list evaluates to true

    // Iterate over queries, stop if any of them eval to true (OR semantics).
    bool result = false;
    for (size_t i = 0; i < queries.size() && !result; ++i) {
        MediaQuery* query = queries[i].get();

        if (mediaTypeMatch(query->mediaType())) {
            const ExpressionVector& expressions = query->expressions();
            // Iterate through expressions, stop if any of them eval to false (AND semantics).
            size_t j = 0;
            for (; j < expressions.size(); ++j) {
                bool exprResult = eval(expressions.at(j).get());
                // FIXME: Instead of storing these on StyleResolver, we should store them locally
                // and then any client of this method can grab at them afterwords.
                // Alternatively we could use an explicit out-parameter of this method.
                if (styleResolver && expressions.at(j)->isViewportDependent())
                    styleResolver->addViewportDependentMediaQueryResult(expressions.at(j).get(), exprResult);
                if (!exprResult)
                    break;
            }

            // Assume true if we are at the end of the list, otherwise assume false.
            result = applyRestrictor(query->restrictor(), expressions.size() == j);
        } else
            result = applyRestrictor(query->restrictor(), false);
    }

    return result;
}

template<typename T>
bool compareValue(T a, T b, MediaFeaturePrefix op)
{
    switch (op) {
    case MinPrefix:
        return a >= b;
    case MaxPrefix:
        return a <= b;
    case NoPrefix:
        return a == b;
    }
    return false;
}

static bool compareAspectRatioValue(CSSValue* value, int width, int height, MediaFeaturePrefix op)
{
    if (value->isAspectRatioValue()) {
        CSSAspectRatioValue* aspectRatio = toCSSAspectRatioValue(value);
        return compareValue(width * static_cast<int>(aspectRatio->denominatorValue()), height * static_cast<int>(aspectRatio->numeratorValue()), op);
    }

    return false;
}

static bool numberValue(CSSValue* value, float& result)
{
    if (value->isPrimitiveValue()
        && toCSSPrimitiveValue(value)->isNumber()) {
        result = toCSSPrimitiveValue(value)->getFloatValue(CSSPrimitiveValue::CSS_NUMBER);
        return true;
    }
    return false;
}

static bool colorMediaFeatureEval(CSSValue* value, RenderStyle*, Frame* frame, MediaFeaturePrefix op)
{
    int bitsPerComponent = screenDepthPerComponent(frame->page()->mainFrame()->view());
    float number;
    if (value)
        return numberValue(value, number) && compareValue(bitsPerComponent, static_cast<int>(number), op);

    return bitsPerComponent != 0;
}

static bool colorIndexMediaFeatureEval(CSSValue* value, RenderStyle*, Frame*, MediaFeaturePrefix op)
{
    // FIXME: We currently assume that we do not support indexed displays, as it is unknown
    // how to retrieve the information if the display mode is indexed. This matches Firefox.
    if (!value)
        return false;

    float number;
    return numberValue(value, number) && compareValue(0, static_cast<int>(number), op);
}

static bool monochromeMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix op)
{
    if (!screenIsMonochrome(frame->page()->mainFrame()->view())) {
        if (value) {
            float number;
            return numberValue(value, number) && compareValue(0, static_cast<int>(number), op);
        }
        return false;
    }

    return colorMediaFeatureEval(value, style, frame, op);
}

static IntSize viewportSize(FrameView* view)
{
    return view->layoutSize(ScrollableArea::IncludeScrollbars);
}

static bool orientationMediaFeatureEval(CSSValue* value, RenderStyle*, Frame* frame, MediaFeaturePrefix)
{
    FrameView* view = frame->view();
    int width = viewportSize(view).width();
    int height = viewportSize(view).height();
    if (value && value->isPrimitiveValue()) {
        const CSSValueID id = toCSSPrimitiveValue(value)->getValueID();
        if (width > height) // Square viewport is portrait.
            return CSSValueLandscape == id;
        return CSSValuePortrait == id;
    }

    // Expression (orientation) evaluates to true if width and height >= 0.
    return height >= 0 && width >= 0;
}

static bool aspectRatioMediaFeatureEval(CSSValue* value, RenderStyle*, Frame* frame, MediaFeaturePrefix op)
{
    if (value) {
        FrameView* view = frame->view();
        return compareAspectRatioValue(value, viewportSize(view).width(), viewportSize(view).height(), op);
    }

    // ({,min-,max-}aspect-ratio)
    // assume if we have a device, its aspect ratio is non-zero
    return true;
}

static bool deviceAspectRatioMediaFeatureEval(CSSValue* value, RenderStyle*, Frame* frame, MediaFeaturePrefix op)
{
    if (value) {
        FloatRect sg = screenRect(frame->page()->mainFrame()->view());
        return compareAspectRatioValue(value, static_cast<int>(sg.width()), static_cast<int>(sg.height()), op);
    }

    // ({,min-,max-}device-aspect-ratio)
    // assume if we have a device, its aspect ratio is non-zero
    return true;
}

static bool evalResolution(CSSValue* value, Frame* frame, MediaFeaturePrefix op)
{
    // FIXME: Possibly handle other media types than 'screen' and 'print'.
    float actualResolution = 0;

    // This checks the actual media type applied to the document, and we know
    // this method only got called if this media type matches the one defined
    // in the query. Thus, if if the document's media type is "print", the
    // media type of the query will either be "print" or "all".
    String mediaType = frame->view()->mediaType();
    if (equalIgnoringCase(mediaType, "screen"))
        actualResolution = clampTo<float>(frame->devicePixelRatio());
    else if (equalIgnoringCase(mediaType, "print")) {
        // The resolution of images while printing should not depend on the DPI
        // of the screen. Until we support proper ways of querying this info
        // we use 300px which is considered minimum for current printers.
        actualResolution = 300 / cssPixelsPerInch;
    }

    if (!value)
        return !!actualResolution;

    if (!value->isPrimitiveValue())
        return false;

    CSSPrimitiveValue* resolution = toCSSPrimitiveValue(value);

    if (resolution->isNumber())
        return compareValue(actualResolution, resolution->getFloatValue(), op);

    if (!resolution->isResolution())
        return false;

    if (resolution->isDotsPerCentimeter()) {
        // To match DPCM to DPPX values, we limit to 2 decimal points.
        // The http://dev.w3.org/csswg/css3-values/#absolute-lengths recommends
        // "that the pixel unit refer to the whole number of device pixels that best
        // approximates the reference pixel". With that in mind, allowing 2 decimal
        // point precision seems appropriate.
        return compareValue(
            floorf(0.5 + 100 * actualResolution) / 100,
            floorf(0.5 + 100 * resolution->getFloatValue(CSSPrimitiveValue::CSS_DPPX)) / 100, op);
    }

    return compareValue(actualResolution, resolution->getFloatValue(CSSPrimitiveValue::CSS_DPPX), op);
}

static bool devicePixelRatioMediaFeatureEval(CSSValue *value, RenderStyle*, Frame* frame, MediaFeaturePrefix op)
{
    return (!value || toCSSPrimitiveValue(value)->isNumber()) && evalResolution(value, frame, op);
}

static bool resolutionMediaFeatureEval(CSSValue* value, RenderStyle*, Frame* frame, MediaFeaturePrefix op)
{
    return (!value || toCSSPrimitiveValue(value)->isResolution()) && evalResolution(value, frame, op);
}

static bool gridMediaFeatureEval(CSSValue* value, RenderStyle*, Frame*, MediaFeaturePrefix op)
{
    // if output device is bitmap, grid: 0 == true
    // assume we have bitmap device
    float number;
    if (value && numberValue(value, number))
        return compareValue(static_cast<int>(number), 0, op);
    return false;
}

static bool computeLength(CSSValue* value, bool strict, RenderStyle* initialStyle, int& result)
{
    if (!value->isPrimitiveValue())
        return false;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue->isNumber()) {
        result = primitiveValue->getIntValue();
        return !strict || !result;
    }

    if (primitiveValue->isLength()) {
        // Relative (like EM) and root relative (like REM) units are always resolved against the initial values
        // for media queries, hence the two initialStyle parameters.
        result = primitiveValue->computeLength<int>(initialStyle, initialStyle, 1.0 /* multiplier */, true /* computingFontSize */);
        return true;
    }

    return false;
}

static bool deviceHeightMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix op)
{
    if (value) {
        FloatRect sg = screenRect(frame->page()->mainFrame()->view());
        int length;
        long height = sg.height();
        InspectorInstrumentation::applyScreenHeightOverride(frame, &height);
        return computeLength(value, !frame->document()->inQuirksMode(), style, length) && compareValue(static_cast<int>(height), length, op);
    }
    // ({,min-,max-}device-height)
    // assume if we have a device, assume non-zero
    return true;
}

static bool deviceWidthMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix op)
{
    if (value) {
        FloatRect sg = screenRect(frame->page()->mainFrame()->view());
        int length;
        long width = sg.width();
        InspectorInstrumentation::applyScreenWidthOverride(frame, &width);
        return computeLength(value, !frame->document()->inQuirksMode(), style, length) && compareValue(static_cast<int>(width), length, op);
    }
    // ({,min-,max-}device-width)
    // assume if we have a device, assume non-zero
    return true;
}

static bool heightMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix op)
{
    FrameView* view = frame->view();

    int height = viewportSize(view).height();
    if (value) {
        if (RenderView* renderView = frame->document()->renderView())
            height = adjustForAbsoluteZoom(height, renderView);
        int length;
        return computeLength(value, !frame->document()->inQuirksMode(), style, length) && compareValue(height, length, op);
    }

    return height;
}

static bool widthMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix op)
{
    FrameView* view = frame->view();

    int width = viewportSize(view).width();
    if (value) {
        if (RenderView* renderView = frame->document()->renderView())
            width = adjustForAbsoluteZoom(width, renderView);
        int length;
        return computeLength(value, !frame->document()->inQuirksMode(), style, length) && compareValue(width, length, op);
    }

    return width;
}

// rest of the functions are trampolines which set the prefix according to the media feature expression used

static bool minColorMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return colorMediaFeatureEval(value, style, frame, MinPrefix);
}

static bool maxColorMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return colorMediaFeatureEval(value, style, frame, MaxPrefix);
}

static bool minColorIndexMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return colorIndexMediaFeatureEval(value, style, frame, MinPrefix);
}

static bool maxColorIndexMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return colorIndexMediaFeatureEval(value, style, frame, MaxPrefix);
}

static bool minMonochromeMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return monochromeMediaFeatureEval(value, style, frame, MinPrefix);
}

static bool maxMonochromeMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return monochromeMediaFeatureEval(value, style, frame, MaxPrefix);
}

static bool minAspectRatioMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return aspectRatioMediaFeatureEval(value, style, frame, MinPrefix);
}

static bool maxAspectRatioMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return aspectRatioMediaFeatureEval(value, style, frame, MaxPrefix);
}

static bool minDeviceAspectRatioMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return deviceAspectRatioMediaFeatureEval(value, style, frame, MinPrefix);
}

static bool maxDeviceAspectRatioMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return deviceAspectRatioMediaFeatureEval(value, style, frame, MaxPrefix);
}

static bool minDevicePixelRatioMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return devicePixelRatioMediaFeatureEval(value, style, frame, MinPrefix);
}

static bool maxDevicePixelRatioMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return devicePixelRatioMediaFeatureEval(value, style, frame, MaxPrefix);
}

static bool minHeightMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return heightMediaFeatureEval(value, style, frame, MinPrefix);
}

static bool maxHeightMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return heightMediaFeatureEval(value, style, frame, MaxPrefix);
}

static bool minWidthMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return widthMediaFeatureEval(value, style, frame, MinPrefix);
}

static bool maxWidthMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return widthMediaFeatureEval(value, style, frame, MaxPrefix);
}

static bool minDeviceHeightMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return deviceHeightMediaFeatureEval(value, style, frame, MinPrefix);
}

static bool maxDeviceHeightMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return deviceHeightMediaFeatureEval(value, style, frame, MaxPrefix);
}

static bool minDeviceWidthMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return deviceWidthMediaFeatureEval(value, style, frame, MinPrefix);
}

static bool maxDeviceWidthMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return deviceWidthMediaFeatureEval(value, style, frame, MaxPrefix);
}

static bool minResolutionMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return resolutionMediaFeatureEval(value, style, frame, MinPrefix);
}

static bool maxResolutionMediaFeatureEval(CSSValue* value, RenderStyle* style, Frame* frame, MediaFeaturePrefix)
{
    return resolutionMediaFeatureEval(value, style, frame, MaxPrefix);
}

static bool animationMediaFeatureEval(CSSValue* value, RenderStyle*, Frame*, MediaFeaturePrefix op)
{
    if (value) {
        float number;
        return numberValue(value, number) && compareValue(1, static_cast<int>(number), op);
    }
    return true;
}

static bool deprecatedTransitionMediaFeatureEval(CSSValue* value, RenderStyle*, Frame* frame, MediaFeaturePrefix op)
{
    UseCounter::countDeprecation(frame->document(), UseCounter::PrefixedTransitionMediaFeature);

    if (value) {
        float number;
        return numberValue(value, number) && compareValue(1, static_cast<int>(number), op);
    }
    return true;
}

static bool transform2dMediaFeatureEval(CSSValue* value, RenderStyle*, Frame*, MediaFeaturePrefix op)
{
    if (value) {
        float number;
        return numberValue(value, number) && compareValue(1, static_cast<int>(number), op);
    }
    return true;
}

static bool transform3dMediaFeatureEval(CSSValue* value, RenderStyle*, Frame* frame, MediaFeaturePrefix op)
{
    bool returnValueIfNoParameter;
    int have3dRendering;

    bool threeDEnabled = false;
    if (RenderView* view = frame->contentRenderer())
        threeDEnabled = view->compositor()->canRender3DTransforms();

    returnValueIfNoParameter = threeDEnabled;
    have3dRendering = threeDEnabled ? 1 : 0;

    if (value) {
        float number;
        return numberValue(value, number) && compareValue(have3dRendering, static_cast<int>(number), op);
    }
    return returnValueIfNoParameter;
}

static bool viewModeMediaFeatureEval(CSSValue* value, RenderStyle*, Frame* frame, MediaFeaturePrefix op)
{
    UNUSED_PARAM(op);
    if (!value)
        return true;

    return toCSSPrimitiveValue(value)->getValueID() == CSSValueWindowed;
}

enum PointerDeviceType { TouchPointer, MousePointer, NoPointer, UnknownPointer };

static PointerDeviceType leastCapablePrimaryPointerDeviceType(Frame* frame)
{
    if (frame->settings()->deviceSupportsTouch())
        return TouchPointer;

    // FIXME: We should also try to determine if we know we have a mouse.
    // When we do this, we'll also need to differentiate between known not to
    // have mouse or touch screen (NoPointer) and unknown (UnknownPointer).
    // We could also take into account other preferences like accessibility
    // settings to decide which of the available pointers should be considered
    // "primary".

    return UnknownPointer;
}

static bool hoverMediaFeatureEval(CSSValue* value, RenderStyle*, Frame* frame, MediaFeaturePrefix)
{
    PointerDeviceType pointer = leastCapablePrimaryPointerDeviceType(frame);

    // If we're on a port that hasn't explicitly opted into providing pointer device information
    // (or otherwise can't be confident in the pointer hardware available), then behave exactly
    // as if this feature feature isn't supported.
    if (pointer == UnknownPointer)
        return false;

    float number = 1;
    if (value) {
        if (!numberValue(value, number))
            return false;
    }

    return (pointer == NoPointer && !number)
        || (pointer == TouchPointer && !number)
        || (pointer == MousePointer && number == 1);
}

static bool pointerMediaFeatureEval(CSSValue* value, RenderStyle*, Frame* frame, MediaFeaturePrefix)
{
    PointerDeviceType pointer = leastCapablePrimaryPointerDeviceType(frame);

    // If we're on a port that hasn't explicitly opted into providing pointer device information
    // (or otherwise can't be confident in the pointer hardware available), then behave exactly
    // as if this feature feature isn't supported.
    if (pointer == UnknownPointer)
        return false;

    if (!value)
        return pointer != NoPointer;

    if (!value->isPrimitiveValue())
        return false;

    const CSSValueID id = toCSSPrimitiveValue(value)->getValueID();
    return (pointer == NoPointer && id == CSSValueNone)
        || (pointer == TouchPointer && id == CSSValueCoarse)
        || (pointer == MousePointer && id == CSSValueFine);
}

static bool scanMediaFeatureEval(CSSValue* value, RenderStyle*, Frame* frame, MediaFeaturePrefix)
{
    // Scan only applies to tv media.
    if (!equalIgnoringCase(frame->view()->mediaType(), "tv"))
        return false;

    if (!value)
        return true;

    if (!value->isPrimitiveValue())
        return false;

    // If a platform interface supplies progressive/interlace info for TVs in the
    // future, it needs to be handled here. For now, assume a modern TV with
    // progressive display.
    return toCSSPrimitiveValue(value)->getValueID() == CSSValueProgressive;
}

static void createFunctionMap()
{
    // Create the table.
    gFunctionMap = new FunctionMap;
#define ADD_TO_FUNCTIONMAP(name, str)  \
    gFunctionMap->set(name##MediaFeature.impl(), name##MediaFeatureEval);
    CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(ADD_TO_FUNCTIONMAP);
#undef ADD_TO_FUNCTIONMAP
}

bool MediaQueryEvaluator::eval(const MediaQueryExp* expr) const
{
    if (!m_frame || !m_style)
        return m_expResult;

    if (!gFunctionMap)
        createFunctionMap();

    // call the media feature evaluation function. Assume no prefix
    // and let trampoline functions override the prefix if prefix is
    // used
    EvalFunc func = gFunctionMap->get(expr->mediaFeature().impl());
    if (func)
        return func(expr->value(), m_style.get(), m_frame, NoPrefix);

    return false;
}

} // namespace
