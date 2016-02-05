/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012, 2013 Intel Corporation. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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
 */

#include "modules/canvas2d/CanvasRenderingContext2D.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/CSSPropertyNames.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/StyleEngine.h"
#include "core/events/Event.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/html/TextMetrics.h"
#include "core/html/canvas/CanvasFontCache.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutTheme.h"
#include "modules/canvas2d/CanvasGradient.h"
#include "modules/canvas2d/CanvasPattern.h"
#include "modules/canvas2d/CanvasRenderingContext2DState.h"
#include "modules/canvas2d/CanvasStyle.h"
#include "modules/canvas2d/HitRegion.h"
#include "modules/canvas2d/Path2D.h"
#include "platform/fonts/FontCache.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/graphics/DrawLooperBuilder.h"
#include "platform/graphics/ExpensiveCanvasHeuristicParameters.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StrokeData.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/text/BidiTextRun.h"
#include "public/platform/Platform.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "wtf/ArrayBufferContents.h"
#include "wtf/CheckedArithmetic.h"
#include "wtf/MathExtras.h"
#include "wtf/OwnPtr.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

static const char defaultFont[] = "10px sans-serif";
static const char inherit[] = "inherit";
static const char rtl[] = "rtl";
static const char ltr[] = "ltr";
static const double TryRestoreContextInterval = 0.5;
static const unsigned MaxTryRestoreContextAttempts = 4;
static const double cDeviceScaleFactor = 1.0; // Canvas is device independent

static bool contextLostRestoredEventsEnabled()
{
    return RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled();
}

// Drawing methods need to use this instead of SkAutoCanvasRestore in case overdraw
// detection substitutes the recording canvas (to discard overdrawn draw calls).
class CanvasRenderingContext2DAutoRestoreSkCanvas {
    STACK_ALLOCATED();
public:
    explicit CanvasRenderingContext2DAutoRestoreSkCanvas(CanvasRenderingContext2D* context)
        : m_context(context)
        , m_saveCount(0)
    {
        ASSERT(m_context);
        SkCanvas* c = m_context->drawingCanvas();
        if (c) {
            m_saveCount = c->getSaveCount();
        }
    }

    ~CanvasRenderingContext2DAutoRestoreSkCanvas()
    {
        SkCanvas* c = m_context->drawingCanvas();
        if (c)
            c->restoreToCount(m_saveCount);
        m_context->validateStateStack();
    }
private:
    RawPtrWillBeMember<CanvasRenderingContext2D> m_context;
    int m_saveCount;
};

CanvasRenderingContext2D::CanvasRenderingContext2D(HTMLCanvasElement* canvas, const CanvasContextCreationAttributes& attrs, Document& document)
    : CanvasRenderingContext(canvas)
    , m_clipAntialiasing(NotAntiAliased)
    , m_hasAlpha(attrs.alpha())
    , m_contextLostMode(NotLostContext)
    , m_contextRestorable(true)
    , m_tryRestoreContextAttemptCount(0)
    , m_dispatchContextLostEventTimer(this, &CanvasRenderingContext2D::dispatchContextLostEvent)
    , m_dispatchContextRestoredEventTimer(this, &CanvasRenderingContext2D::dispatchContextRestoredEvent)
    , m_tryRestoreContextEventTimer(this, &CanvasRenderingContext2D::tryRestoreContextEvent)
    , m_pruneLocalFontCacheScheduled(false)
{
    if (document.settings() && document.settings()->antialiasedClips2dCanvasEnabled())
        m_clipAntialiasing = AntiAliased;
    m_stateStack.append(CanvasRenderingContext2DState::create());
    setShouldAntialias(true);
}

void CanvasRenderingContext2D::unwindStateStack()
{
    if (size_t stackSize = m_stateStack.size()) {
        if (SkCanvas* skCanvas = canvas()->existingDrawingCanvas()) {
            while (--stackSize)
                skCanvas->restore();
        }
    }
}

CanvasRenderingContext2D::~CanvasRenderingContext2D()
{
    if (m_pruneLocalFontCacheScheduled) {
        Platform::current()->currentThread()->removeTaskObserver(this);
    }
}

void CanvasRenderingContext2D::validateStateStack()
{
#if ENABLE(ASSERT)
    SkCanvas* skCanvas = canvas()->existingDrawingCanvas();
    if (skCanvas && m_contextLostMode == NotLostContext) {
        ASSERT(static_cast<size_t>(skCanvas->getSaveCount()) == m_stateStack.size());
    }
#endif
}

CanvasRenderingContext2DState& CanvasRenderingContext2D::modifiableState()
{
    realizeSaves();
    return *m_stateStack.last();
}

bool CanvasRenderingContext2D::isAccelerated() const
{
    if (!canvas()->hasImageBuffer())
        return false;
    return canvas()->buffer()->isAccelerated();
}

void CanvasRenderingContext2D::stop()
{
    if (!isContextLost()) {
        // Never attempt to restore the context because the page is being torn down.
        loseContext(SyntheticLostContext);
    }
}

bool CanvasRenderingContext2D::isContextLost() const
{
    return m_contextLostMode != NotLostContext;
}

void CanvasRenderingContext2D::loseContext(LostContextMode lostMode)
{
    if (m_contextLostMode != NotLostContext)
        return;
    m_contextLostMode = lostMode;
    if (m_contextLostMode == SyntheticLostContext) {
        canvas()->discardImageBuffer();
    }
    m_dispatchContextLostEventTimer.startOneShot(0, BLINK_FROM_HERE);
}

void CanvasRenderingContext2D::didSetSurfaceSize()
{
    if (!m_contextRestorable)
        return;
    // This code path is for restoring from an eviction
    // Restoring from surface failure is handled internally
    ASSERT(m_contextLostMode != NotLostContext && !canvas()->hasImageBuffer());

    if (canvas()->buffer()) {
        if (contextLostRestoredEventsEnabled()) {
            m_dispatchContextRestoredEventTimer.startOneShot(0, BLINK_FROM_HERE);
        } else {
            // legacy synchronous context restoration.
            reset();
            m_contextLostMode = NotLostContext;
        }
    }
}

DEFINE_TRACE(CanvasRenderingContext2D)
{
    visitor->trace(m_stateStack);
    visitor->trace(m_hitRegionManager);
    CanvasRenderingContext::trace(visitor);
}

void CanvasRenderingContext2D::dispatchContextLostEvent(Timer<CanvasRenderingContext2D>*)
{
    if (contextLostRestoredEventsEnabled()) {
        RefPtrWillBeRawPtr<Event> event = Event::createCancelable(EventTypeNames::contextlost);
        canvas()->dispatchEvent(event);
        if (event->defaultPrevented()) {
            m_contextRestorable = false;
        }
    }

    // If RealLostContext, it means the context was not lost due to surface failure
    // but rather due to a an eviction, which means image buffer exists.
    if (m_contextRestorable && m_contextLostMode == RealLostContext) {
        m_tryRestoreContextAttemptCount = 0;
        m_tryRestoreContextEventTimer.startRepeating(TryRestoreContextInterval, BLINK_FROM_HERE);
    }
}

void CanvasRenderingContext2D::tryRestoreContextEvent(Timer<CanvasRenderingContext2D>* timer)
{
    if (m_contextLostMode == NotLostContext) {
        // Canvas was already restored (possibly thanks to a resize), so stop trying.
        m_tryRestoreContextEventTimer.stop();
        return;
    }

    ASSERT(m_contextLostMode == RealLostContext);
    if (canvas()->hasImageBuffer() && canvas()->buffer()->restoreSurface()) {
        m_tryRestoreContextEventTimer.stop();
        dispatchContextRestoredEvent(nullptr);
    }

    if (++m_tryRestoreContextAttemptCount > MaxTryRestoreContextAttempts) {
        // final attempt: allocate a brand new image buffer instead of restoring
        canvas()->discardImageBuffer();
        m_tryRestoreContextEventTimer.stop();
        if (canvas()->buffer())
            dispatchContextRestoredEvent(nullptr);
    }
}

void CanvasRenderingContext2D::dispatchContextRestoredEvent(Timer<CanvasRenderingContext2D>*)
{
    if (m_contextLostMode == NotLostContext)
        return;
    reset();
    m_contextLostMode = NotLostContext;
    if (contextLostRestoredEventsEnabled()) {
        RefPtrWillBeRawPtr<Event> event(Event::create(EventTypeNames::contextrestored));
        canvas()->dispatchEvent(event);
    }
}

void CanvasRenderingContext2D::reset()
{
    validateStateStack();
    unwindStateStack();
    m_stateStack.resize(1);
    m_stateStack.first() = CanvasRenderingContext2DState::create();
    m_path.clear();
    SkCanvas* c = canvas()->existingDrawingCanvas();
    if (c) {
        c->resetMatrix();
        c->clipRect(SkRect::MakeWH(canvas()->width(), canvas()->height()), SkRegion::kReplace_Op);
    }
    validateStateStack();
}

void CanvasRenderingContext2D::restoreCanvasMatrixClipStack(SkCanvas* c) const
{
    if (!c)
        return;
    WillBeHeapVector<OwnPtrWillBeMember<CanvasRenderingContext2DState>>::const_iterator currState;
    ASSERT(m_stateStack.begin() < m_stateStack.end());
    for (currState = m_stateStack.begin(); currState < m_stateStack.end(); currState++) {
        c->setMatrix(SkMatrix::I());
        currState->get()->playbackClips(c);
        c->setMatrix(affineTransformToSkMatrix(currState->get()->transform()));
        c->save();
    }
    c->restore();
}

void CanvasRenderingContext2D::realizeSaves()
{
    validateStateStack();
    if (state().hasUnrealizedSaves()) {
        ASSERT(m_stateStack.size() >= 1);
        // Reduce the current state's unrealized count by one now,
        // to reflect the fact we are saving one state.
        m_stateStack.last()->restore();
        m_stateStack.append(CanvasRenderingContext2DState::create(state(), CanvasRenderingContext2DState::DontCopyClipList));
        // Set the new state's unrealized count to 0, because it has no outstanding saves.
        // We need to do this explicitly because the copy constructor and operator= used
        // by the Vector operations copy the unrealized count from the previous state (in
        // turn necessary to support correct resizing and unwinding of the stack).
        m_stateStack.last()->resetUnrealizedSaveCount();
        SkCanvas* canvas = drawingCanvas();
        if (canvas)
            canvas->save();
        validateStateStack();
    }
}

void CanvasRenderingContext2D::save()
{
    m_stateStack.last()->save();
}

void CanvasRenderingContext2D::restore()
{
    validateStateStack();
    if (state().hasUnrealizedSaves()) {
        // We never realized the save, so just record that it was unnecessary.
        m_stateStack.last()->restore();
        return;
    }
    ASSERT(m_stateStack.size() >= 1);
    if (m_stateStack.size() <= 1)
        return;
    m_path.transform(state().transform());
    m_stateStack.removeLast();
    m_path.transform(state().transform().inverse());
    SkCanvas* c = drawingCanvas();
    if (c)
        c->restore();

    validateStateStack();
}

static inline void convertCanvasStyleToUnionType(CanvasStyle* style, StringOrCanvasGradientOrCanvasPattern& returnValue)
{
    if (CanvasGradient* gradient = style->canvasGradient()) {
        returnValue.setCanvasGradient(gradient);
        return;
    }
    if (CanvasPattern* pattern = style->canvasPattern()) {
        returnValue.setCanvasPattern(pattern);
        return;
    }
    returnValue.setString(style->color());
}

void CanvasRenderingContext2D::strokeStyle(StringOrCanvasGradientOrCanvasPattern& returnValue) const
{
    convertCanvasStyleToUnionType(state().strokeStyle(), returnValue);
}

void CanvasRenderingContext2D::setStrokeStyle(const StringOrCanvasGradientOrCanvasPattern& style)
{
    ASSERT(!style.isNull());

    String colorString;
    CanvasStyle* canvasStyle = nullptr;
    if (style.isString()) {
        colorString = style.getAsString();
        if (colorString == state().unparsedStrokeColor())
            return;
        Color parsedColor = 0;
        if (!parseColorOrCurrentColor(parsedColor, colorString, canvas()))
            return;
        if (state().strokeStyle()->isEquivalentRGBA(parsedColor.rgb())) {
            modifiableState().setUnparsedStrokeColor(colorString);
            return;
        }
        canvasStyle = CanvasStyle::createFromRGBA(parsedColor.rgb());
    } else if (style.isCanvasGradient()) {
        canvasStyle = CanvasStyle::createFromGradient(style.getAsCanvasGradient());
    } else if (style.isCanvasPattern()) {
        CanvasPattern* canvasPattern = style.getAsCanvasPattern();

        if (canvas()->originClean() && !canvasPattern->originClean())
            canvas()->setOriginTainted();

        canvasStyle = CanvasStyle::createFromPattern(canvasPattern);
    }

    ASSERT(canvasStyle);

    modifiableState().setStrokeStyle(canvasStyle);
    modifiableState().setUnparsedStrokeColor(colorString);
}

void CanvasRenderingContext2D::fillStyle(StringOrCanvasGradientOrCanvasPattern& returnValue) const
{
    convertCanvasStyleToUnionType(state().fillStyle(), returnValue);
}

void CanvasRenderingContext2D::setFillStyle(const StringOrCanvasGradientOrCanvasPattern& style)
{
    ASSERT(!style.isNull());
    validateStateStack();
    String colorString;
    CanvasStyle* canvasStyle = nullptr;
    if (style.isString()) {
        colorString = style.getAsString();
        if (colorString == state().unparsedFillColor())
            return;
        Color parsedColor = 0;
        if (!parseColorOrCurrentColor(parsedColor, colorString, canvas()))
            return;
        if (state().fillStyle()->isEquivalentRGBA(parsedColor.rgb())) {
            modifiableState().setUnparsedFillColor(colorString);
            return;
        }
        canvasStyle = CanvasStyle::createFromRGBA(parsedColor.rgb());
    } else if (style.isCanvasGradient()) {
        canvasStyle = CanvasStyle::createFromGradient(style.getAsCanvasGradient());
    } else if (style.isCanvasPattern()) {
        CanvasPattern* canvasPattern = style.getAsCanvasPattern();

        if (canvas()->originClean() && !canvasPattern->originClean())
            canvas()->setOriginTainted();
        if (canvasPattern->pattern()->isTextureBacked())
            canvas()->disableDeferral();
        canvasStyle = CanvasStyle::createFromPattern(canvasPattern);
    }

    ASSERT(canvasStyle);
    modifiableState().setFillStyle(canvasStyle);
    modifiableState().setUnparsedFillColor(colorString);
}

double CanvasRenderingContext2D::lineWidth() const
{
    return state().lineWidth();
}

void CanvasRenderingContext2D::setLineWidth(double width)
{
    if (!std::isfinite(width) || width <= 0)
        return;
    if (state().lineWidth() == width)
        return;
    modifiableState().setLineWidth(width);
}

String CanvasRenderingContext2D::lineCap() const
{
    return lineCapName(state().lineCap());
}

void CanvasRenderingContext2D::setLineCap(const String& s)
{
    LineCap cap;
    if (!parseLineCap(s, cap))
        return;
    if (state().lineCap() == cap)
        return;
    modifiableState().setLineCap(cap);
}

String CanvasRenderingContext2D::lineJoin() const
{
    return lineJoinName(state().lineJoin());
}

void CanvasRenderingContext2D::setLineJoin(const String& s)
{
    LineJoin join;
    if (!parseLineJoin(s, join))
        return;
    if (state().lineJoin() == join)
        return;
    modifiableState().setLineJoin(join);
}

double CanvasRenderingContext2D::miterLimit() const
{
    return state().miterLimit();
}

void CanvasRenderingContext2D::setMiterLimit(double limit)
{
    if (!std::isfinite(limit) || limit <= 0)
        return;
    if (state().miterLimit() == limit)
        return;
    modifiableState().setMiterLimit(limit);
}

double CanvasRenderingContext2D::shadowOffsetX() const
{
    return state().shadowOffset().width();
}

void CanvasRenderingContext2D::setShadowOffsetX(double x)
{
    if (!std::isfinite(x))
        return;
    if (state().shadowOffset().width() == x)
        return;
    modifiableState().setShadowOffsetX(x);
}

double CanvasRenderingContext2D::shadowOffsetY() const
{
    return state().shadowOffset().height();
}

void CanvasRenderingContext2D::setShadowOffsetY(double y)
{
    if (!std::isfinite(y))
        return;
    if (state().shadowOffset().height() == y)
        return;
    modifiableState().setShadowOffsetY(y);
}

double CanvasRenderingContext2D::shadowBlur() const
{
    return state().shadowBlur();
}

void CanvasRenderingContext2D::setShadowBlur(double blur)
{
    if (!std::isfinite(blur) || blur < 0)
        return;
    if (state().shadowBlur() == blur)
        return;
    modifiableState().setShadowBlur(blur);
}

String CanvasRenderingContext2D::shadowColor() const
{
    return Color(state().shadowColor()).serialized();
}

void CanvasRenderingContext2D::setShadowColor(const String& colorString)
{
    Color color;
    if (!parseColorOrCurrentColor(color, colorString, canvas()))
        return;
    if (state().shadowColor() == color)
        return;
    modifiableState().setShadowColor(color.rgb());
}

const Vector<double>& CanvasRenderingContext2D::getLineDash() const
{
    return state().lineDash();
}

static bool lineDashSequenceIsValid(const Vector<double>& dash)
{
    for (size_t i = 0; i < dash.size(); i++) {
        if (!std::isfinite(dash[i]) || dash[i] < 0)
            return false;
    }
    return true;
}

void CanvasRenderingContext2D::setLineDash(const Vector<double>& dash)
{
    if (!lineDashSequenceIsValid(dash))
        return;
    modifiableState().setLineDash(dash);
}

double CanvasRenderingContext2D::lineDashOffset() const
{
    return state().lineDashOffset();
}

void CanvasRenderingContext2D::setLineDashOffset(double offset)
{
    if (!std::isfinite(offset) || state().lineDashOffset() == offset)
        return;
    modifiableState().setLineDashOffset(offset);
}

double CanvasRenderingContext2D::globalAlpha() const
{
    return state().globalAlpha();
}

void CanvasRenderingContext2D::setGlobalAlpha(double alpha)
{
    if (!(alpha >= 0 && alpha <= 1))
        return;
    if (state().globalAlpha() == alpha)
        return;
    modifiableState().setGlobalAlpha(alpha);
}

bool CanvasRenderingContext2D::shouldAntialias() const
{
    return state().shouldAntialias();
}

void CanvasRenderingContext2D::setShouldAntialias(bool doAA)
{
    modifiableState().setShouldAntialias(doAA);
}

String CanvasRenderingContext2D::globalCompositeOperation() const
{
    return compositeOperatorName(compositeOperatorFromSkia(state().globalComposite()), blendModeFromSkia(state().globalComposite()));
}

void CanvasRenderingContext2D::setGlobalCompositeOperation(const String& operation)
{
    CompositeOperator op = CompositeSourceOver;
    WebBlendMode blendMode = WebBlendModeNormal;
    if (!parseCompositeAndBlendOperator(operation, op, blendMode))
        return;
    SkXfermode::Mode xfermode = WebCoreCompositeToSkiaComposite(op, blendMode);
    if (state().globalComposite() == xfermode)
        return;
    modifiableState().setGlobalComposite(xfermode);
}

String CanvasRenderingContext2D::filter() const
{
    return state().unparsedFilter();
}

void CanvasRenderingContext2D::setFilter(const String& filterString)
{
    if (filterString == state().unparsedFilter())
        return;

    RefPtrWillBeRawPtr<CSSValue> filterValue = CSSParser::parseSingleValue(CSSPropertyWebkitFilter, filterString, CSSParserContext(HTMLStandardMode, 0));

    if (!filterValue || filterValue->isInitialValue() || filterValue->isInheritedValue())
        return;

    modifiableState().setUnparsedFilter(filterString);
    modifiableState().setFilter(filterValue.release());
}

PassRefPtrWillBeRawPtr<SVGMatrixTearOff> CanvasRenderingContext2D::currentTransform() const
{
    return SVGMatrixTearOff::create(state().transform());
}

void CanvasRenderingContext2D::setCurrentTransform(PassRefPtrWillBeRawPtr<SVGMatrixTearOff> passMatrixTearOff)
{
    RefPtrWillBeRawPtr<SVGMatrixTearOff> matrixTearOff = passMatrixTearOff;
    const AffineTransform& transform = matrixTearOff->value();
    setTransform(transform.a(), transform.b(), transform.c(), transform.d(), transform.e(), transform.f());
}

void CanvasRenderingContext2D::scale(double sx, double sy)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    if (!std::isfinite(sx) || !std::isfinite(sy))
        return;

    AffineTransform newTransform = state().transform();
    newTransform.scaleNonUniform(sx, sy);
    if (state().transform() == newTransform)
        return;

    modifiableState().setTransform(newTransform);
    if (!state().isTransformInvertible())
        return;

    c->scale(sx, sy);
    m_path.transform(AffineTransform().scaleNonUniform(1.0 / sx, 1.0 / sy));
}

void CanvasRenderingContext2D::rotate(double angleInRadians)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    if (!std::isfinite(angleInRadians))
        return;

    AffineTransform newTransform = state().transform();
    newTransform.rotateRadians(angleInRadians);
    if (state().transform() == newTransform)
        return;

    modifiableState().setTransform(newTransform);
    if (!state().isTransformInvertible())
        return;
    c->rotate(angleInRadians * (180.0 / piFloat));
    m_path.transform(AffineTransform().rotateRadians(-angleInRadians));
}

void CanvasRenderingContext2D::translate(double tx, double ty)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;
    if (!state().isTransformInvertible())
        return;

    if (!std::isfinite(tx) || !std::isfinite(ty))
        return;

    AffineTransform newTransform = state().transform();
    newTransform.translate(tx, ty);
    if (state().transform() == newTransform)
        return;

    modifiableState().setTransform(newTransform);
    if (!state().isTransformInvertible())
        return;
    c->translate(tx, ty);
    m_path.transform(AffineTransform().translate(-tx, -ty));
}

void CanvasRenderingContext2D::transform(double m11, double m12, double m21, double m22, double dx, double dy)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) || !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy))
        return;

    AffineTransform transform(m11, m12, m21, m22, dx, dy);
    AffineTransform newTransform = state().transform() * transform;
    if (state().transform() == newTransform)
        return;

    modifiableState().setTransform(newTransform);
    if (!state().isTransformInvertible())
        return;

    c->concat(affineTransformToSkMatrix(transform));
    m_path.transform(transform.inverse());
}

void CanvasRenderingContext2D::resetTransform()
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    AffineTransform ctm = state().transform();
    bool invertibleCTM = state().isTransformInvertible();
    // It is possible that CTM is identity while CTM is not invertible.
    // When CTM becomes non-invertible, realizeSaves() can make CTM identity.
    if (ctm.isIdentity() && invertibleCTM)
        return;

    // resetTransform() resolves the non-invertible CTM state.
    modifiableState().resetTransform();
    c->setMatrix(affineTransformToSkMatrix(canvas()->baseTransform()));

    if (invertibleCTM)
        m_path.transform(ctm);
    // When else, do nothing because all transform methods didn't update m_path when CTM became non-invertible.
    // It means that resetTransform() restores m_path just before CTM became non-invertible.
}

void CanvasRenderingContext2D::setTransform(double m11, double m12, double m21, double m22, double dx, double dy)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) || !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy))
        return;

    resetTransform();
    transform(m11, m12, m21, m22, dx, dy);
}

void CanvasRenderingContext2D::beginPath()
{
    m_path.clear();
}

static bool validateRectForCanvas(double& x, double& y, double& width, double& height)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height))
        return false;

    if (!width && !height)
        return false;

    if (width < 0) {
        width = -width;
        x -= width;
    }

    if (height < 0) {
        height = -height;
        y -= height;
    }

    return true;
}

static bool isFullCanvasCompositeMode(SkXfermode::Mode op)
{
    // See 4.8.11.1.3 Compositing
    // CompositeSourceAtop and CompositeDestinationOut are not listed here as the platforms already
    // implement the specification's behavior.
    return op == SkXfermode::kSrcIn_Mode || op == SkXfermode::kSrcOut_Mode || op == SkXfermode::kDstIn_Mode || op == SkXfermode::kDstATop_Mode;
}

template<typename DrawFunc>
void CanvasRenderingContext2D::compositedDraw(const DrawFunc& drawFunc, SkCanvas* c, CanvasRenderingContext2DState::PaintType paintType, CanvasRenderingContext2DState::ImageType imageType)
{
    SkImageFilter* filter = state().getFilter(canvas(), accessFont(), canvas()->size());
    ASSERT(isFullCanvasCompositeMode(state().globalComposite()) || filter);
    SkMatrix ctm = c->getTotalMatrix();
    c->resetMatrix();
    SkPaint compositePaint;
    compositePaint.setXfermodeMode(state().globalComposite());
    if (state().shouldDrawShadows()) {
        // unroll into two independently composited passes if drawing shadows
        SkPaint shadowPaint = *state().getPaint(paintType, DrawShadowOnly, imageType);
        int saveCount = c->getSaveCount();
        if (filter) {
            SkPaint filterPaint;
            filterPaint.setImageFilter(filter);
            // TODO(junov): crbug.com/502921 We could use primitive bounds if we knew that the filter
            // does not affect transparent black regions.
            c->saveLayer(nullptr, &shadowPaint);
            c->saveLayer(nullptr, &filterPaint);
            SkPaint foregroundPaint = *state().getPaint(paintType, DrawForegroundOnly, imageType);
            c->setMatrix(ctm);
            drawFunc(c, &foregroundPaint);
        } else {
            ASSERT(isFullCanvasCompositeMode(state().globalComposite()));
            c->saveLayer(nullptr, &compositePaint);
            shadowPaint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
            c->setMatrix(ctm);
            drawFunc(c, &shadowPaint);
        }
        c->restoreToCount(saveCount);
    }

    compositePaint.setImageFilter(filter);
    // TODO(junov): crbug.com/502921 We could use primitive bounds if we knew that the filter
    // does not affect transparent black regions *and* !isFullCanvasCompositeMode
    c->saveLayer(nullptr, &compositePaint);
    SkPaint foregroundPaint = *state().getPaint(paintType, DrawForegroundOnly, imageType);
    foregroundPaint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
    c->setMatrix(ctm);
    drawFunc(c, &foregroundPaint);
    c->restore();
    c->setMatrix(ctm);
}

template<typename DrawFunc, typename ContainsFunc>
bool CanvasRenderingContext2D::draw(const DrawFunc& drawFunc, const ContainsFunc& drawCoversClipBounds, const SkRect& bounds, CanvasRenderingContext2DState::PaintType paintType, CanvasRenderingContext2DState::ImageType imageType)
{
    if (!state().isTransformInvertible())
        return false;

    SkIRect clipBounds;
    if (!drawingCanvas() || !drawingCanvas()->getClipDeviceBounds(&clipBounds))
        return false;

    // If gradient size is zero, then paint nothing.
    CanvasStyle* style = state().style(paintType);
    if (style) {
        CanvasGradient* gradient = style->canvasGradient();
        if (gradient && gradient->gradient()->isZeroSize())
            return false;
    }

    if (isFullCanvasCompositeMode(state().globalComposite()) || state().hasFilter(canvas(), accessFont(), canvas()->size())) {
        compositedDraw(drawFunc, drawingCanvas(), paintType, imageType);
        didDraw(clipBounds);
    } else if (state().globalComposite() == SkXfermode::kSrc_Mode) {
        clearCanvas(); // takes care of checkOverdraw()
        const SkPaint* paint = state().getPaint(paintType, DrawForegroundOnly, imageType);
        drawFunc(drawingCanvas(), paint);
        didDraw(clipBounds);
    } else {
        SkIRect dirtyRect;
        if (computeDirtyRect(bounds, clipBounds, &dirtyRect)) {
            const SkPaint* paint = state().getPaint(paintType, DrawShadowAndForeground, imageType);
            if (paintType != CanvasRenderingContext2DState::StrokePaintType && drawCoversClipBounds(clipBounds))
                checkOverdraw(bounds, paint, imageType, ClipFill);
            drawFunc(drawingCanvas(), paint);
            didDraw(dirtyRect);
        }
    }
    return true;
}

static bool isPathExpensive(const Path& path)
{
    const SkPath& skPath = path.skPath();
    if (ExpensiveCanvasHeuristicParameters::ConcavePathsAreExpensive && !skPath.isConvex())
        return true;

    if (skPath.countPoints() > ExpensiveCanvasHeuristicParameters::ExpensivePathPointCount)
        return true;

    return false;
}

void CanvasRenderingContext2D::drawPathInternal(const Path& path, CanvasRenderingContext2DState::PaintType paintType, SkPath::FillType fillType)
{
    if (path.isEmpty())
        return;

    SkPath skPath = path.skPath();
    FloatRect bounds = path.boundingRect();
    skPath.setFillType(fillType);

    if (paintType == CanvasRenderingContext2DState::StrokePaintType)
        inflateStrokeRect(bounds);

    if (!drawingCanvas())
        return;

    if (draw(
        [&skPath, this](SkCanvas* c, const SkPaint* paint) // draw lambda
        {
            c->drawPath(skPath, *paint);
        },
        [](const SkIRect& rect) // overdraw test lambda
        {
            return false;
        }, bounds, paintType)) {
        if (isPathExpensive(path)) {
            ImageBuffer* buffer = canvas()->buffer();
            if (buffer)
                buffer->setHasExpensiveOp();
        }
    }
}

static SkPath::FillType parseWinding(const String& windingRuleString)
{
    if (windingRuleString == "nonzero")
        return SkPath::kWinding_FillType;
    if (windingRuleString == "evenodd")
        return SkPath::kEvenOdd_FillType;

    ASSERT_NOT_REACHED();
    return SkPath::kEvenOdd_FillType;
}

void CanvasRenderingContext2D::fill(const String& windingRuleString)
{
    drawPathInternal(m_path, CanvasRenderingContext2DState::FillPaintType, parseWinding(windingRuleString));
}

void CanvasRenderingContext2D::fill(Path2D* domPath, const String& windingRuleString)
{
    drawPathInternal(domPath->path(), CanvasRenderingContext2DState::FillPaintType, parseWinding(windingRuleString));
}

void CanvasRenderingContext2D::stroke()
{
    drawPathInternal(m_path, CanvasRenderingContext2DState::StrokePaintType);
}

void CanvasRenderingContext2D::stroke(Path2D* domPath)
{
    drawPathInternal(domPath->path(), CanvasRenderingContext2DState::StrokePaintType);
}

void CanvasRenderingContext2D::fillRect(double x, double y, double width, double height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    if (!drawingCanvas())
        return;

    SkRect rect = SkRect::MakeXYWH(x, y, width, height);
    draw(
        [&rect, this](SkCanvas* c, const SkPaint* paint) // draw lambda
        {
            c->drawRect(rect, *paint);
        },
        [&rect, this](const SkIRect& clipBounds) // overdraw test lambda
        {
            return rectContainsTransformedRect(rect, clipBounds);
        }, rect, CanvasRenderingContext2DState::FillPaintType);
}

static void strokeRectOnCanvas(const FloatRect& rect, SkCanvas* canvas, const SkPaint* paint)
{
    ASSERT(paint->getStyle() == SkPaint::kStroke_Style);
    if ((rect.width() > 0) != (rect.height() > 0)) {
        // When stroking, we must skip the zero-dimension segments
        SkPath path;
        path.moveTo(rect.x(), rect.y());
        path.lineTo(rect.maxX(), rect.maxY());
        path.close();
        canvas->drawPath(path, *paint);
        return;
    }
    canvas->drawRect(rect, *paint);
}

void CanvasRenderingContext2D::strokeRect(double x, double y, double width, double height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    if (!drawingCanvas())
        return;

    SkRect rect = SkRect::MakeXYWH(x, y, width, height);
    FloatRect bounds = rect;
    inflateStrokeRect(bounds);
    draw(
        [&rect, this](SkCanvas* c, const SkPaint* paint) // draw lambda
        {
            strokeRectOnCanvas(rect, c, paint);
        },
        [](const SkIRect& clipBounds) // overdraw test lambda
        {
            return false;
        }, bounds, CanvasRenderingContext2DState::StrokePaintType);
}

void CanvasRenderingContext2D::clipInternal(const Path& path, const String& windingRuleString)
{
    SkCanvas* c = drawingCanvas();
    if (!c) {
        return;
    }
    if (!state().isTransformInvertible()) {
        return;
    }

    SkPath skPath = path.skPath();
    skPath.setFillType(parseWinding(windingRuleString));
    modifiableState().clipPath(skPath, m_clipAntialiasing);
    c->clipPath(skPath, SkRegion::kIntersect_Op, m_clipAntialiasing == AntiAliased);
    if (ExpensiveCanvasHeuristicParameters::ComplexClipsAreExpensive && !skPath.isRect(0) && canvas()->hasImageBuffer()) {
        canvas()->buffer()->setHasExpensiveOp();
    }
}

void CanvasRenderingContext2D::clip(const String& windingRuleString)
{
    clipInternal(m_path, windingRuleString);
}

void CanvasRenderingContext2D::clip(Path2D* domPath, const String& windingRuleString)
{
    clipInternal(domPath->path(), windingRuleString);
}

bool CanvasRenderingContext2D::isPointInPath(const double x, const double y, const String& windingRuleString)
{
    return isPointInPathInternal(m_path, x, y, windingRuleString);
}

bool CanvasRenderingContext2D::isPointInPath(Path2D* domPath, const double x, const double y, const String& windingRuleString)
{
    return isPointInPathInternal(domPath->path(), x, y, windingRuleString);
}

bool CanvasRenderingContext2D::isPointInPathInternal(const Path& path, const double x, const double y, const String& windingRuleString)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return false;
    if (!state().isTransformInvertible())
        return false;

    FloatPoint point(x, y);
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
        return false;
    AffineTransform ctm = state().transform();
    FloatPoint transformedPoint = ctm.inverse().mapPoint(point);

    return path.contains(transformedPoint, SkFillTypeToWindRule(parseWinding(windingRuleString)));
}

bool CanvasRenderingContext2D::isPointInStroke(const double x, const double y)
{
    return isPointInStrokeInternal(m_path, x, y);
}

bool CanvasRenderingContext2D::isPointInStroke(Path2D* domPath, const double x, const double y)
{
    return isPointInStrokeInternal(domPath->path(), x, y);
}

bool CanvasRenderingContext2D::isPointInStrokeInternal(const Path& path, const double x, const double y)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return false;
    if (!state().isTransformInvertible())
        return false;

    FloatPoint point(x, y);
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
        return false;
    AffineTransform ctm = state().transform();
    FloatPoint transformedPoint = ctm.inverse().mapPoint(point);

    StrokeData strokeData;
    strokeData.setThickness(state().lineWidth());
    strokeData.setLineCap(state().lineCap());
    strokeData.setLineJoin(state().lineJoin());
    strokeData.setMiterLimit(state().miterLimit());
    Vector<float> lineDash(state().lineDash().size());
    std::copy(state().lineDash().begin(), state().lineDash().end(), lineDash.begin());
    strokeData.setLineDash(lineDash, state().lineDashOffset());
    return path.strokeContains(transformedPoint, strokeData);
}

void CanvasRenderingContext2D::scrollPathIntoView()
{
    scrollPathIntoViewInternal(m_path);
}

void CanvasRenderingContext2D::scrollPathIntoView(Path2D* path2d)
{
    scrollPathIntoViewInternal(path2d->path());
}

void CanvasRenderingContext2D::scrollPathIntoViewInternal(const Path& path)
{
    if (!state().isTransformInvertible() || path.isEmpty())
        return;

    canvas()->document().updateLayoutIgnorePendingStylesheets();

    LayoutObject* renderer = canvas()->layoutObject();
    LayoutBox* layoutBox = canvas()->layoutBox();
    if (!renderer || !layoutBox)
        return;

    // Apply transformation and get the bounding rect
    Path transformedPath = path;
    transformedPath.transform(state().transform());
    FloatRect boundingRect = transformedPath.boundingRect();

    // Offset by the canvas rect
    LayoutRect pathRect(boundingRect);
    IntRect canvasRect = layoutBox->absoluteContentBox();
    pathRect.moveBy(canvasRect.location());

    renderer->scrollRectToVisible(
        pathRect, ScrollAlignment::alignCenterAlways, ScrollAlignment::alignTopAlways);

    // TODO: should implement "inform the user" that the caret and/or
    // selection the specified rectangle of the canvas. See http://crbug.com/357987
}

void CanvasRenderingContext2D::clearRect(double x, double y, double width, double height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    SkCanvas* c = drawingCanvas();
    if (!c)
        return;
    if (!state().isTransformInvertible())
        return;

    SkIRect clipBounds;
    if (!c->getClipDeviceBounds(&clipBounds))
        return;

    SkPaint clearPaint;
    clearPaint.setXfermodeMode(SkXfermode::kClear_Mode);
    clearPaint.setStyle(SkPaint::kFill_Style);
    FloatRect rect(x, y, width, height);

    if (rectContainsTransformedRect(rect, clipBounds)) {
        checkOverdraw(rect, &clearPaint, CanvasRenderingContext2DState::NoImage, ClipFill);
        if (drawingCanvas())
            drawingCanvas()->drawRect(rect, clearPaint);
        didDraw(clipBounds);
    } else {
        SkIRect dirtyRect;
        if (computeDirtyRect(rect, clipBounds, &dirtyRect)) {
            c->drawRect(rect, clearPaint);
            didDraw(dirtyRect);
        }
    }

    if (m_hitRegionManager) {
        m_hitRegionManager->removeHitRegionsInRect(rect, state().transform());
    }
}

static inline FloatRect normalizeRect(const FloatRect& rect)
{
    return FloatRect(std::min(rect.x(), rect.maxX()),
        std::min(rect.y(), rect.maxY()),
        std::max(rect.width(), -rect.width()),
        std::max(rect.height(), -rect.height()));
}

static inline void clipRectsToImageRect(const FloatRect& imageRect, FloatRect* srcRect, FloatRect* dstRect)
{
    if (imageRect.contains(*srcRect))
        return;

    // Compute the src to dst transform
    FloatSize scale(dstRect->size().width() / srcRect->size().width(), dstRect->size().height() / srcRect->size().height());
    FloatPoint scaledSrcLocation = srcRect->location();
    scaledSrcLocation.scale(scale.width(), scale.height());
    FloatSize offset = dstRect->location() - scaledSrcLocation;

    srcRect->intersect(imageRect);

    // To clip the destination rectangle in the same proportion, transform the clipped src rect
    *dstRect = *srcRect;
    dstRect->scale(scale.width(), scale.height());
    dstRect->move(offset);
}

static inline CanvasImageSource* toImageSourceInternal(const CanvasImageSourceUnion& value)
{
    if (value.isHTMLImageElement())
        return value.getAsHTMLImageElement().get();
    if (value.isHTMLVideoElement())
        return value.getAsHTMLVideoElement().get();
    if (value.isHTMLCanvasElement())
        return value.getAsHTMLCanvasElement().get();
    if (value.isImageBitmap())
        return value.getAsImageBitmap().get();
    ASSERT_NOT_REACHED();
    return nullptr;
}

void CanvasRenderingContext2D::drawImage(const CanvasImageSourceUnion& imageSource, double x, double y, ExceptionState& exceptionState)
{
    CanvasImageSource* imageSourceInternal = toImageSourceInternal(imageSource);
    FloatSize sourceRectSize = imageSourceInternal->elementSize();
    FloatSize destRectSize = imageSourceInternal->defaultDestinationSize();
    drawImage(imageSourceInternal, 0, 0, sourceRectSize.width(), sourceRectSize.height(), x, y, destRectSize.width(), destRectSize.height(), exceptionState);
}

void CanvasRenderingContext2D::drawImage(const CanvasImageSourceUnion& imageSource,
    double x, double y, double width, double height, ExceptionState& exceptionState)
{
    CanvasImageSource* imageSourceInternal = toImageSourceInternal(imageSource);
    FloatSize sourceRectSize = imageSourceInternal->elementSize();
    drawImage(imageSourceInternal, 0, 0, sourceRectSize.width(), sourceRectSize.height(), x, y, width, height, exceptionState);
}

void CanvasRenderingContext2D::drawImage(const CanvasImageSourceUnion& imageSource,
    double sx, double sy, double sw, double sh,
    double dx, double dy, double dw, double dh, ExceptionState& exceptionState)
{
    CanvasImageSource* imageSourceInternal = toImageSourceInternal(imageSource);
    drawImage(imageSourceInternal, sx, sy, sw, sh, dx, dy, dw, dh, exceptionState);
}

bool CanvasRenderingContext2D::shouldDrawImageAntialiased(const FloatRect& destRect) const
{
    if (!shouldAntialias())
        return false;
    SkCanvas* c = drawingCanvas();
    ASSERT(c);

    const SkMatrix &ctm = c->getTotalMatrix();
    // Don't disable anti-aliasing if we're rotated or skewed.
    if (!ctm.rectStaysRect())
        return true;
    // Check if the dimensions of the destination are "small" (less than one
    // device pixel). To prevent sudden drop-outs. Since we know that
    // kRectStaysRect_Mask is set, the matrix either has scale and no skew or
    // vice versa. We can query the kAffine_Mask flag to determine which case
    // it is.
    // FIXME: This queries the CTM while drawing, which is generally
    // discouraged. Always drawing with AA can negatively impact performance
    // though - that's why it's not always on.
    SkScalar widthExpansion, heightExpansion;
    if (ctm.getType() & SkMatrix::kAffine_Mask)
        widthExpansion = ctm[SkMatrix::kMSkewY], heightExpansion = ctm[SkMatrix::kMSkewX];
    else
        widthExpansion = ctm[SkMatrix::kMScaleX], heightExpansion = ctm[SkMatrix::kMScaleY];
    return destRect.width() * fabs(widthExpansion) < 1 || destRect.height() * fabs(heightExpansion) < 1;
}

void CanvasRenderingContext2D::drawImageInternal(SkCanvas* c, CanvasImageSource* imageSource, Image* image, const FloatRect& srcRect, const FloatRect& dstRect, const SkPaint* paint)
{
    int initialSaveCount = c->getSaveCount();
    SkPaint imagePaint = *paint;

    if (paint->getImageFilter()) {
        SkMatrix invCtm;
        if (!c->getTotalMatrix().invert(&invCtm)) {
            // There is an earlier check for invertibility, but the arithmetic
            // in AffineTransform is not exactly identical, so it is possible
            // for SkMatrix to find the transform to be non-invertible at this stage.
            // crbug.com/504687
            return;
        }
        SkRect bounds = dstRect;
        SkPaint layerPaint;
        layerPaint.setXfermode(paint->getXfermode());
        SkAutoTUnref<SkImageFilter> localFilter(paint->getImageFilter()->newWithLocalMatrix(invCtm));
        layerPaint.setImageFilter(localFilter);
        c->saveLayer(&bounds, &layerPaint);
        imagePaint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
        imagePaint.setImageFilter(nullptr);
    }

    if (!imageSource->isVideoElement()) {
        imagePaint.setAntiAlias(shouldDrawImageAntialiased(dstRect));
        image->draw(c, imagePaint, dstRect, srcRect, DoNotRespectImageOrientation, Image::DoNotClampImageToSourceRect);
    } else {
        c->save();
        c->clipRect(dstRect);
        c->translate(dstRect.x(), dstRect.y());
        c->scale(dstRect.width() / srcRect.width(), dstRect.height() / srcRect.height());
        c->translate(-srcRect.x(), -srcRect.y());
        HTMLVideoElement* video = static_cast<HTMLVideoElement*>(imageSource);
        video->paintCurrentFrame(c, IntRect(IntPoint(), IntSize(video->videoWidth(), video->videoHeight())), &imagePaint);
    }

    c->restoreToCount(initialSaveCount);
}

bool shouldDisableDeferral(CanvasImageSource* imageSource)
{
    if (imageSource->isVideoElement())
        return true;
    if (imageSource->isCanvasElement()) {
        HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(imageSource);
        if (canvas->isAnimated2D())
            return true;
    }
    return false;
}

void CanvasRenderingContext2D::drawImage(CanvasImageSource* imageSource,
    double sx, double sy, double sw, double sh,
    double dx, double dy, double dw, double dh, ExceptionState& exceptionState)
{
    if (!drawingCanvas())
        return;

    RefPtr<Image> image;
    SourceImageStatus sourceImageStatus = InvalidSourceImageStatus;
    if (!imageSource->isVideoElement()) {
        AccelerationHint hint = canvas()->buffer()->isAccelerated() ? PreferAcceleration : PreferNoAcceleration;
        image = imageSource->getSourceImageForCanvas(&sourceImageStatus, hint);
        if (sourceImageStatus == UndecodableSourceImageStatus)
            exceptionState.throwDOMException(InvalidStateError, "The HTMLImageElement provided is in the 'broken' state.");
        if (!image || !image->width() || !image->height())
            return;
    } else {
        if (!static_cast<HTMLVideoElement*>(imageSource)->hasAvailableVideoFrame())
            return;
    }

    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dw) || !std::isfinite(dh)
        || !std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(sw) || !std::isfinite(sh)
        || !dw || !dh || !sw || !sh)
        return;

    FloatRect srcRect = normalizeRect(FloatRect(sx, sy, sw, sh));
    FloatRect dstRect = normalizeRect(FloatRect(dx, dy, dw, dh));

    clipRectsToImageRect(FloatRect(FloatPoint(), imageSource->elementSize()), &srcRect, &dstRect);

    imageSource->adjustDrawRects(&srcRect, &dstRect);

    if (srcRect.isEmpty())
        return;

    if (shouldDisableDeferral(imageSource) || image->isTextureBacked())
        canvas()->disableDeferral();

    validateStateStack();

    draw(
        [this, &imageSource, &image, &srcRect, dstRect](SkCanvas* c, const SkPaint* paint) // draw lambda
        {
            drawImageInternal(c, imageSource, image.get(), srcRect, dstRect, paint);
        },
        [this, &dstRect](const SkIRect& clipBounds) // overdraw test lambda
        {
            return rectContainsTransformedRect(dstRect, clipBounds);
        }, dstRect, CanvasRenderingContext2DState::ImagePaintType,
        imageSource->isOpaque() ? CanvasRenderingContext2DState::OpaqueImage : CanvasRenderingContext2DState::NonOpaqueImage);

    validateStateStack();

    bool isExpensive = false;

    if (ExpensiveCanvasHeuristicParameters::SVGImageSourcesAreExpensive && imageSource->isSVGSource())
        isExpensive = true;

    if (imageSource->elementSize().width() * imageSource->elementSize().height() > canvas()->width() * canvas()->height() * ExpensiveCanvasHeuristicParameters::ExpensiveImageSizeRatio)
        isExpensive = true;

    if (isExpensive) {
        ImageBuffer* buffer = canvas()->buffer();
        if (buffer)
            buffer->setHasExpensiveOp();
    }

    if (imageSource->isCanvasElement() && static_cast<HTMLCanvasElement*>(imageSource)->is3D()) {
        // WebGL to 2D canvas: must flush graphics context to prevent a race
        // FIXME: crbug.com/516331 Fix the underlying synchronization issue so this flush can be eliminated.
        canvas()->buffer()->flushGpu();
    }

    if (canvas()->originClean() && wouldTaintOrigin(imageSource))
        canvas()->setOriginTainted();
}

void CanvasRenderingContext2D::clearCanvas()
{
    FloatRect canvasRect(0, 0, canvas()->width(), canvas()->height());
    checkOverdraw(canvasRect, 0, CanvasRenderingContext2DState::NoImage, ClipFill);
    SkCanvas* c = drawingCanvas();
    if (c)
        c->clear(m_hasAlpha ? SK_ColorTRANSPARENT : SK_ColorBLACK);
}

bool CanvasRenderingContext2D::rectContainsTransformedRect(const FloatRect& rect, const SkIRect& transformedRect) const
{
    FloatQuad quad(rect);
    FloatQuad transformedQuad(FloatRect(transformedRect.x(), transformedRect.y(), transformedRect.width(), transformedRect.height()));
    return state().transform().mapQuad(quad).containsQuad(transformedQuad);
}

CanvasGradient* CanvasRenderingContext2D::createLinearGradient(double x0, double y0, double x1, double y1)
{
    CanvasGradient* gradient = CanvasGradient::create(FloatPoint(x0, y0), FloatPoint(x1, y1));
    return gradient;
}

CanvasGradient* CanvasRenderingContext2D::createRadialGradient(double x0, double y0, double r0, double x1, double y1, double r1, ExceptionState& exceptionState)
{
    if (r0 < 0 || r1 < 0) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The %s provided is less than 0.", r0 < 0 ? "r0" : "r1"));
        return nullptr;
    }

    CanvasGradient* gradient = CanvasGradient::create(FloatPoint(x0, y0), r0, FloatPoint(x1, y1), r1);
    return gradient;
}

CanvasPattern* CanvasRenderingContext2D::createPattern(const CanvasImageSourceUnion& imageSource, const String& repetitionType, ExceptionState& exceptionState)
{
    Pattern::RepeatMode repeatMode = CanvasPattern::parseRepetitionType(repetitionType, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    SourceImageStatus status;
    CanvasImageSource* imageSourceInternal = toImageSourceInternal(imageSource);
    RefPtr<Image> imageForRendering = imageSourceInternal->getSourceImageForCanvas(&status, PreferNoAcceleration);

    switch (status) {
    case NormalSourceImageStatus:
        break;
    case ZeroSizeCanvasSourceImageStatus:
        exceptionState.throwDOMException(InvalidStateError, String::format("The canvas %s is 0.", imageSourceInternal->elementSize().width() ? "height" : "width"));
        return nullptr;
    case UndecodableSourceImageStatus:
        exceptionState.throwDOMException(InvalidStateError, "Source image is in the 'broken' state.");
        return nullptr;
    case InvalidSourceImageStatus:
        imageForRendering = Image::nullImage();
        break;
    case IncompleteSourceImageStatus:
        return nullptr;
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    ASSERT(imageForRendering);

    bool originClean = !wouldTaintOrigin(imageSourceInternal);

    return CanvasPattern::create(imageForRendering.release(), repeatMode, originClean);
}

bool CanvasRenderingContext2D::computeDirtyRect(const FloatRect& localRect, SkIRect* dirtyRect)
{
    SkIRect clipBounds;
    if (!drawingCanvas()->getClipDeviceBounds(&clipBounds))
        return false;
    return computeDirtyRect(localRect, clipBounds, dirtyRect);
}

bool CanvasRenderingContext2D::computeDirtyRect(const FloatRect& localRect, const SkIRect& transformedClipBounds, SkIRect* dirtyRect)
{
    FloatRect canvasRect = state().transform().mapRect(localRect);

    if (alphaChannel(state().shadowColor())) {
        FloatRect shadowRect(canvasRect);
        shadowRect.move(state().shadowOffset());
        shadowRect.inflate(state().shadowBlur());
        canvasRect.unite(shadowRect);
    }

    SkIRect canvasIRect;
    static_cast<SkRect>(canvasRect).roundOut(&canvasIRect);
    if (!canvasIRect.intersect(transformedClipBounds))
        return false;

    if (dirtyRect)
        *dirtyRect = canvasIRect;

    return true;
}

void CanvasRenderingContext2D::didDraw(const SkIRect& dirtyRect)
{
    if (dirtyRect.isEmpty())
        return;

    if (ExpensiveCanvasHeuristicParameters::BlurredShadowsAreExpensive && state().shouldDrawShadows() && state().shadowBlur() > 0) {
        ImageBuffer* buffer = canvas()->buffer();
        if (buffer)
            buffer->setHasExpensiveOp();
    }

    canvas()->didDraw(SkRect::Make(dirtyRect));
}

SkCanvas* CanvasRenderingContext2D::drawingCanvas() const
{
    if (isContextLost())
        return nullptr;
    return canvas()->drawingCanvas();
}

ImageData* CanvasRenderingContext2D::createImageData(ImageData* imageData) const
{
    return ImageData::create(imageData->size());
}

ImageData* CanvasRenderingContext2D::createImageData(double sw, double sh, ExceptionState& exceptionState) const
{
    if (!sw || !sh) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s is 0.", sw ? "height" : "width"));
        return nullptr;
    }

    FloatSize logicalSize(fabs(sw), fabs(sh));
    if (!logicalSize.isExpressibleAsIntSize())
        return nullptr;

    IntSize size = expandedIntSize(logicalSize);
    if (size.width() < 1)
        size.setWidth(1);
    if (size.height() < 1)
        size.setHeight(1);

    return ImageData::create(size);
}

ImageData* CanvasRenderingContext2D::getImageData(double sx, double sy, double sw, double sh, ExceptionState& exceptionState) const
{
    if (!canvas()->originClean())
        exceptionState.throwSecurityError("The canvas has been tainted by cross-origin data.");
    else if (!sw || !sh)
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s is 0.", sw ? "height" : "width"));

    if (exceptionState.hadException())
        return nullptr;

    if (sw < 0) {
        sx += sw;
        sw = -sw;
    }
    if (sh < 0) {
        sy += sh;
        sh = -sh;
    }

    FloatRect logicalRect(sx, sy, sw, sh);
    if (logicalRect.width() < 1)
        logicalRect.setWidth(1);
    if (logicalRect.height() < 1)
        logicalRect.setHeight(1);
    if (!logicalRect.isExpressibleAsIntRect())
        return nullptr;

    IntRect imageDataRect = enclosingIntRect(logicalRect);
    ImageBuffer* buffer = canvas()->buffer();
    if (!buffer || isContextLost())
        return ImageData::create(imageDataRect.size());

    WTF::ArrayBufferContents contents;
    if (!buffer->getImageData(Unmultiplied, imageDataRect, contents))
        return nullptr;

    RefPtr<DOMArrayBuffer> arrayBuffer = DOMArrayBuffer::create(contents);
    return ImageData::create(
        imageDataRect.size(),
        DOMUint8ClampedArray::create(arrayBuffer, 0, arrayBuffer->byteLength()));
}

void CanvasRenderingContext2D::putImageData(ImageData* data, double dx, double dy, ExceptionState& exceptionState)
{
    putImageData(data, dx, dy, 0, 0, data->width(), data->height(), exceptionState);
}

void CanvasRenderingContext2D::putImageData(ImageData* data, double dx, double dy, double dirtyX, double dirtyY, double dirtyWidth, double dirtyHeight, ExceptionState& exceptionState)
{
    if (data->data()->bufferBase()->isNeutered()) {
        exceptionState.throwDOMException(InvalidStateError, "The source data has been neutered.");
        return;
    }
    ImageBuffer* buffer = canvas()->buffer();
    if (!buffer)
        return;

    if (dirtyWidth < 0) {
        dirtyX += dirtyWidth;
        dirtyWidth = -dirtyWidth;
    }

    if (dirtyHeight < 0) {
        dirtyY += dirtyHeight;
        dirtyHeight = -dirtyHeight;
    }

    FloatRect clipRect(dirtyX, dirtyY, dirtyWidth, dirtyHeight);
    clipRect.intersect(IntRect(0, 0, data->width(), data->height()));
    IntSize destOffset(static_cast<int>(dx), static_cast<int>(dy));
    IntRect destRect = enclosingIntRect(clipRect);
    destRect.move(destOffset);
    destRect.intersect(IntRect(IntPoint(), buffer->size()));
    if (destRect.isEmpty())
        return;
    IntRect sourceRect(destRect);
    sourceRect.move(-destOffset);

    checkOverdraw(destRect, 0, CanvasRenderingContext2DState::NoImage, UntransformedUnclippedFill);

    buffer->putByteArray(Unmultiplied, data->data()->data(), IntSize(data->width(), data->height()), sourceRect, IntPoint(destOffset));

    didDraw(destRect);
}

String CanvasRenderingContext2D::font() const
{
    if (!state().hasRealizedFont())
        return defaultFont;

    canvas()->document().canvasFontCache()->willUseCurrentFont();
    StringBuilder serializedFont;
    const FontDescription& fontDescription = state().font().fontDescription();

    if (fontDescription.style() == FontStyleItalic)
        serializedFont.appendLiteral("italic ");
    if (fontDescription.weight() == FontWeightBold)
        serializedFont.appendLiteral("bold ");
    if (fontDescription.variant() == FontVariantSmallCaps)
        serializedFont.appendLiteral("small-caps ");

    serializedFont.appendNumber(fontDescription.computedPixelSize());
    serializedFont.appendLiteral("px");

    const FontFamily& firstFontFamily = fontDescription.family();
    for (const FontFamily* fontFamily = &firstFontFamily; fontFamily; fontFamily = fontFamily->next()) {
        if (fontFamily != &firstFontFamily)
            serializedFont.append(',');

        // FIXME: We should append family directly to serializedFont rather than building a temporary string.
        String family = fontFamily->family();
        if (family.startsWith("-webkit-"))
            family = family.substring(8);
        if (family.contains(' '))
            family = "\"" + family + "\"";

        serializedFont.append(' ');
        serializedFont.append(family);
    }

    return serializedFont.toString();
}

void CanvasRenderingContext2D::setFont(const String& newFont)
{
    // The style resolution required for rendering text is not available in frame-less documents.
    if (!canvas()->document().frame())
        return;

    canvas()->document().updateLayoutTreeForNodeIfNeeded(canvas());

    // The following early exit is dependent on the cache not being empty
    // because an empty cache may indicate that a style change has occured
    // which would require that the font be re-resolved. This check has to
    // come after the layout tree update to flush pending style changes.
    if (newFont == state().unparsedFont() && state().hasRealizedFont() && m_fontsResolvedUsingCurrentStyle.size() > 0)
        return;

    CanvasFontCache* canvasFontCache = canvas()->document().canvasFontCache();

    // Map the <canvas> font into the text style. If the font uses keywords like larger/smaller, these will work
    // relative to the canvas.
    RefPtr<ComputedStyle> fontStyle;
    const ComputedStyle* computedStyle = canvas()->ensureComputedStyle();
    if (computedStyle) {
        HashMap<String, Font>::iterator i = m_fontsResolvedUsingCurrentStyle.find(newFont);
        if (i != m_fontsResolvedUsingCurrentStyle.end()) {
            ASSERT(m_fontLRUList.contains(newFont));
            m_fontLRUList.remove(newFont);
            m_fontLRUList.add(newFont);
            modifiableState().setFont(i->value, canvas()->document().styleEngine().fontSelector());
        } else {
            MutableStylePropertySet* parsedStyle = canvasFontCache->parseFont(newFont);
            if (!parsedStyle)
                return;
            fontStyle = ComputedStyle::create();
            FontDescription elementFontDescription(computedStyle->fontDescription());
            // Reset the computed size to avoid inheriting the zoom factor from the <canvas> element.
            elementFontDescription.setComputedSize(elementFontDescription.specifiedSize());
            fontStyle->setFontDescription(elementFontDescription);
            fontStyle->font().update(fontStyle->font().fontSelector());
            canvas()->document().ensureStyleResolver().computeFont(fontStyle.get(), *parsedStyle);
            m_fontsResolvedUsingCurrentStyle.add(newFont, fontStyle->font());
            ASSERT(!m_fontLRUList.contains(newFont));
            m_fontLRUList.add(newFont);
            pruneLocalFontCache(canvasFontCache->hardMaxFonts()); // hard limit
            schedulePruneLocalFontCacheIfNeeded(); // soft limit
            modifiableState().setFont(fontStyle->font(), canvas()->document().styleEngine().fontSelector());
        }
    } else {
        Font resolvedFont;
        if (!canvasFontCache->getFontUsingDefaultStyle(newFont, resolvedFont))
            return;
        modifiableState().setFont(resolvedFont, canvas()->document().styleEngine().fontSelector());
    }

    // The parse succeeded.
    String newFontSafeCopy(newFont); // Create a string copy since newFont can be deleted inside realizeSaves.
    modifiableState().setUnparsedFont(newFontSafeCopy);
}

void CanvasRenderingContext2D::schedulePruneLocalFontCacheIfNeeded()
{
    if (m_pruneLocalFontCacheScheduled)
        return;
    m_pruneLocalFontCacheScheduled = true;
    Platform::current()->currentThread()->addTaskObserver(this);
}

void CanvasRenderingContext2D::didProcessTask()
{
    // The rendering surface needs to be prepared now because it will be too late
    // to create a layer once we are in the paint invalidation phase.
    canvas()->prepareSurfaceForPaintingIfNeeded();

    pruneLocalFontCache(canvas()->document().canvasFontCache()->maxFonts());
    m_pruneLocalFontCacheScheduled = false;
    Platform::current()->currentThread()->removeTaskObserver(this);
}

void CanvasRenderingContext2D::pruneLocalFontCache(size_t targetSize)
{
    if (targetSize == 0) {
        // Short cut: LRU does not matter when evicting everything
        m_fontLRUList.clear();
        m_fontsResolvedUsingCurrentStyle.clear();
        return;
    }
    while (m_fontLRUList.size() > targetSize) {
        m_fontsResolvedUsingCurrentStyle.remove(m_fontLRUList.first());
        m_fontLRUList.removeFirst();
    }
}

void CanvasRenderingContext2D::styleDidChange(const ComputedStyle* oldStyle, const ComputedStyle& newStyle)
{
    if (oldStyle && oldStyle->font() == newStyle.font())
        return;
    pruneLocalFontCache(0);
}

String CanvasRenderingContext2D::textAlign() const
{
    return textAlignName(state().textAlign());
}

void CanvasRenderingContext2D::setTextAlign(const String& s)
{
    TextAlign align;
    if (!parseTextAlign(s, align))
        return;
    if (state().textAlign() == align)
        return;
    modifiableState().setTextAlign(align);
}

String CanvasRenderingContext2D::textBaseline() const
{
    return textBaselineName(state().textBaseline());
}

void CanvasRenderingContext2D::setTextBaseline(const String& s)
{
    TextBaseline baseline;
    if (!parseTextBaseline(s, baseline))
        return;
    if (state().textBaseline() == baseline)
        return;
    modifiableState().setTextBaseline(baseline);
}

static inline TextDirection toTextDirection(CanvasRenderingContext2DState::Direction direction, HTMLCanvasElement* canvas, const ComputedStyle** computedStyle = 0)
{
    const ComputedStyle* style = (computedStyle || direction == CanvasRenderingContext2DState::DirectionInherit) ? canvas->ensureComputedStyle() : nullptr;
    if (computedStyle)
        *computedStyle = style;
    switch (direction) {
    case CanvasRenderingContext2DState::DirectionInherit:
        return style ? style->direction() : LTR;
    case CanvasRenderingContext2DState::DirectionRTL:
        return RTL;
    case CanvasRenderingContext2DState::DirectionLTR:
        return LTR;
    }
    ASSERT_NOT_REACHED();
    return LTR;
}

String CanvasRenderingContext2D::direction() const
{
    if (state().direction() == CanvasRenderingContext2DState::DirectionInherit)
        canvas()->document().updateLayoutTreeForNodeIfNeeded(canvas());
    return toTextDirection(state().direction(), canvas()) == RTL ? rtl : ltr;
}

void CanvasRenderingContext2D::setDirection(const String& directionString)
{
    CanvasRenderingContext2DState::Direction direction;
    if (directionString == inherit)
        direction = CanvasRenderingContext2DState::DirectionInherit;
    else if (directionString == rtl)
        direction = CanvasRenderingContext2DState::DirectionRTL;
    else if (directionString == ltr)
        direction = CanvasRenderingContext2DState::DirectionLTR;
    else
        return;

    if (state().direction() == direction)
        return;

    modifiableState().setDirection(direction);
}

void CanvasRenderingContext2D::fillText(const String& text, double x, double y)
{
    drawTextInternal(text, x, y, CanvasRenderingContext2DState::FillPaintType);
}

void CanvasRenderingContext2D::fillText(const String& text, double x, double y, double maxWidth)
{
    drawTextInternal(text, x, y, CanvasRenderingContext2DState::FillPaintType, &maxWidth);
}

void CanvasRenderingContext2D::strokeText(const String& text, double x, double y)
{
    drawTextInternal(text, x, y, CanvasRenderingContext2DState::StrokePaintType);
}

void CanvasRenderingContext2D::strokeText(const String& text, double x, double y, double maxWidth)
{
    drawTextInternal(text, x, y, CanvasRenderingContext2DState::StrokePaintType, &maxWidth);
}

TextMetrics* CanvasRenderingContext2D::measureText(const String& text)
{
    TextMetrics* metrics = TextMetrics::create();

    // The style resolution required for rendering text is not available in frame-less documents.
    if (!canvas()->document().frame())
        return metrics;

    canvas()->document().updateLayoutTreeForNodeIfNeeded(canvas());
    const Font& font = accessFont();

    TextDirection direction;
    if (state().direction() == CanvasRenderingContext2DState::DirectionInherit)
        direction = determineDirectionality(text);
    else
        direction = toTextDirection(state().direction(), canvas());
    TextRun textRun(text, 0, 0, TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion, direction, false);
    textRun.setNormalizeSpace(true);
    FloatRect textBounds = font.selectionRectForText(textRun, FloatPoint(), font.fontDescription().computedSize(), 0, -1, true);

    // x direction
    metrics->setWidth(font.width(textRun));
    metrics->setActualBoundingBoxLeft(-textBounds.x());
    metrics->setActualBoundingBoxRight(textBounds.maxX());

    // y direction
    const FontMetrics& fontMetrics = font.fontMetrics();
    const float ascent = fontMetrics.floatAscent();
    const float descent = fontMetrics.floatDescent();
    const float baselineY = getFontBaseline(fontMetrics);

    metrics->setFontBoundingBoxAscent(ascent - baselineY);
    metrics->setFontBoundingBoxDescent(descent + baselineY);
    metrics->setActualBoundingBoxAscent(-textBounds.y() - baselineY);
    metrics->setActualBoundingBoxDescent(textBounds.maxY() + baselineY);

    // Note : top/bottom and ascend/descend are currently the same, so there's no difference
    //        between the EM box's top and bottom and the font's ascend and descend
    metrics->setEmHeightAscent(0);
    metrics->setEmHeightDescent(0);

    metrics->setHangingBaseline(-0.8f * ascent + baselineY);
    metrics->setAlphabeticBaseline(baselineY);
    metrics->setIdeographicBaseline(descent + baselineY);
    return metrics;
}

void CanvasRenderingContext2D::drawTextInternal(const String& text, double x, double y, CanvasRenderingContext2DState::PaintType paintType, double* maxWidth)
{
    // The style resolution required for rendering text is not available in frame-less documents.
    if (!canvas()->document().frame())
        return;

    // accessFont needs the style to be up to date, but updating style can cause script to run,
    // (e.g. due to autofocus) which can free the canvas (set size to 0, for example), so update
    // style before grabbing the drawingCanvas.
    canvas()->document().updateLayoutTreeForNodeIfNeeded(canvas());

    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    if (maxWidth && (!std::isfinite(*maxWidth) || *maxWidth <= 0))
        return;

    // Currently, SkPictureImageFilter does not support subpixel text anti-aliasing, which
    // is expected when !m_hasAlpha, so we need to fall out of display list mode when
    // drawing text to an opaque canvas
    // crbug.com/583809
    if (!m_hasAlpha && !isAccelerated())
        canvas()->disableDeferral();

    const Font& font = accessFont();
    const FontMetrics& fontMetrics = font.fontMetrics();

    // FIXME: Need to turn off font smoothing.

    const ComputedStyle* computedStyle = 0;
    TextDirection direction = toTextDirection(state().direction(), canvas(), &computedStyle);
    bool isRTL = direction == RTL;
    bool override = computedStyle ? isOverride(computedStyle->unicodeBidi()) : false;

    TextRun textRun(text, 0, 0, TextRun::AllowTrailingExpansion, direction, override);
    textRun.setNormalizeSpace(true);
    // Draw the item text at the correct point.
    FloatPoint location(x, y + getFontBaseline(fontMetrics));
    double fontWidth = font.width(textRun);

    bool useMaxWidth = (maxWidth && *maxWidth < fontWidth);
    double width = useMaxWidth ? *maxWidth : fontWidth;

    TextAlign align = state().textAlign();
    if (align == StartTextAlign)
        align = isRTL ? RightTextAlign : LeftTextAlign;
    else if (align == EndTextAlign)
        align = isRTL ? LeftTextAlign : RightTextAlign;

    switch (align) {
    case CenterTextAlign:
        location.setX(location.x() - width / 2);
        break;
    case RightTextAlign:
        location.setX(location.x() - width);
        break;
    default:
        break;
    }

    // The slop built in to this mask rect matches the heuristic used in FontCGWin.cpp for GDI text.
    TextRunPaintInfo textRunPaintInfo(textRun);
    textRunPaintInfo.bounds = FloatRect(location.x() - fontMetrics.height() / 2,
        location.y() - fontMetrics.ascent() - fontMetrics.lineGap(),
        width + fontMetrics.height(),
        fontMetrics.lineSpacing());
    if (paintType == CanvasRenderingContext2DState::StrokePaintType)
        inflateStrokeRect(textRunPaintInfo.bounds);

    CanvasRenderingContext2DAutoRestoreSkCanvas stateRestorer(this);
    if (useMaxWidth) {
        drawingCanvas()->save();
        drawingCanvas()->translate(location.x(), location.y());
        // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op) still work.
        drawingCanvas()->scale((fontWidth > 0 ? (width / fontWidth) : 0), 1);
        location = FloatPoint();
    }

    draw(
        [&font, this, &textRunPaintInfo, &location](SkCanvas* c, const SkPaint* paint) // draw lambda
        {
            font.drawBidiText(c, textRunPaintInfo, location, Font::UseFallbackIfFontNotReady, cDeviceScaleFactor, *paint);
        },
        [](const SkIRect& rect) // overdraw test lambda
        {
            return false;
        },
        textRunPaintInfo.bounds, paintType);
}

void CanvasRenderingContext2D::inflateStrokeRect(FloatRect& rect) const
{
    // Fast approximation of the stroke's bounding rect.
    // This yields a slightly oversized rect but is very fast
    // compared to Path::strokeBoundingRect().
    static const double root2 = sqrtf(2);
    double delta = state().lineWidth() / 2;
    if (state().lineJoin() == MiterJoin)
        delta *= state().miterLimit();
    else if (state().lineCap() == SquareCap)
        delta *= root2;

    rect.inflate(delta);
}

const Font& CanvasRenderingContext2D::accessFont()
{
    if (!state().hasRealizedFont())
        setFont(state().unparsedFont());
    canvas()->document().canvasFontCache()->willUseCurrentFont();
    return state().font();
}

int CanvasRenderingContext2D::getFontBaseline(const FontMetrics& fontMetrics) const
{
    switch (state().textBaseline()) {
    case TopTextBaseline:
        return fontMetrics.ascent();
    case HangingTextBaseline:
        // According to http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
        // "FOP (Formatting Objects Processor) puts the hanging baseline at 80% of the ascender height"
        return (fontMetrics.ascent() * 4) / 5;
    case BottomTextBaseline:
    case IdeographicTextBaseline:
        return -fontMetrics.descent();
    case MiddleTextBaseline:
        return -fontMetrics.descent() + fontMetrics.height() / 2;
    case AlphabeticTextBaseline:
    default:
        // Do nothing.
        break;
    }
    return 0;
}

void CanvasRenderingContext2D::setIsHidden(bool hidden)
{
    if (canvas()->hasImageBuffer())
        canvas()->buffer()->setIsHidden(hidden);
    if (hidden) {
        pruneLocalFontCache(0);
    }
}

bool CanvasRenderingContext2D::isTransformInvertible() const
{
    return state().isTransformInvertible();
}

WebLayer* CanvasRenderingContext2D::platformLayer() const
{
    return canvas()->buffer() ? canvas()->buffer()->platformLayer() : 0;
}

bool CanvasRenderingContext2D::imageSmoothingEnabled() const
{
    return state().imageSmoothingEnabled();
}

void CanvasRenderingContext2D::setImageSmoothingEnabled(bool enabled)
{
    if (enabled == state().imageSmoothingEnabled())
        return;

    modifiableState().setImageSmoothingEnabled(enabled);
}

String CanvasRenderingContext2D::imageSmoothingQuality() const
{
    return state().imageSmoothingQuality();
}

void CanvasRenderingContext2D::setImageSmoothingQuality(const String& quality)
{
    if (quality == state().imageSmoothingQuality())
        return;

    modifiableState().setImageSmoothingQuality(quality);
}

void CanvasRenderingContext2D::getContextAttributes(Canvas2DContextAttributes& attrs) const
{
    attrs.setAlpha(m_hasAlpha);
}

void CanvasRenderingContext2D::drawFocusIfNeeded(Element* element)
{
    drawFocusIfNeededInternal(m_path, element);
}

void CanvasRenderingContext2D::drawFocusIfNeeded(Path2D* path2d, Element* element)
{
    drawFocusIfNeededInternal(path2d->path(), element);
}

void CanvasRenderingContext2D::drawFocusIfNeededInternal(const Path& path, Element* element)
{
    if (!focusRingCallIsValid(path, element))
        return;

    // Note: we need to check document->focusedElement() rather than just calling
    // element->focused(), because element->focused() isn't updated until after
    // focus events fire.
    if (element->document().focusedElement() == element) {
        scrollPathIntoViewInternal(path);
        drawFocusRing(path);
    }

    // Update its accessible bounds whether it's focused or not.
    updateElementAccessibility(path, element);
}

bool CanvasRenderingContext2D::focusRingCallIsValid(const Path& path, Element* element)
{
    ASSERT(element);
    if (!state().isTransformInvertible())
        return false;
    if (path.isEmpty())
        return false;
    if (!element->isDescendantOf(canvas()))
        return false;

    return true;
}

void CanvasRenderingContext2D::drawFocusRing(const Path& path)
{
    if (!drawingCanvas())
        return;

    SkColor color = LayoutTheme::theme().focusRingColor().rgb();
    const int focusRingWidth = 5;

    drawPlatformFocusRing(path.skPath(), drawingCanvas(), color, focusRingWidth);

    // We need to add focusRingWidth to dirtyRect.
    StrokeData strokeData;
    strokeData.setThickness(focusRingWidth);

    SkIRect dirtyRect;
    if (!computeDirtyRect(path.strokeBoundingRect(strokeData), &dirtyRect))
        return;

    didDraw(dirtyRect);
}

void CanvasRenderingContext2D::updateElementAccessibility(const Path& path, Element* element)
{
    element->document().updateLayoutIgnorePendingStylesheets();
    AXObjectCache* axObjectCache = element->document().existingAXObjectCache();
    LayoutBoxModelObject* lbmo = canvas()->layoutBoxModelObject();
    LayoutObject* renderer = canvas()->layoutObject();
    if (!axObjectCache || !lbmo || !renderer)
        return;

    // Get the transformed path.
    Path transformedPath = path;
    transformedPath.transform(state().transform());

    // Offset by the canvas rect, taking border and padding into account.
    IntRect canvasRect = renderer->absoluteBoundingBoxRect();
    canvasRect.move(lbmo->borderLeft() + lbmo->paddingLeft(), lbmo->borderTop() + lbmo->paddingTop());
    LayoutRect elementRect = enclosingLayoutRect(transformedPath.boundingRect());
    elementRect.moveBy(canvasRect.location());
    axObjectCache->setCanvasObjectBounds(element, elementRect);
}

void CanvasRenderingContext2D::addHitRegion(const HitRegionOptions& options, ExceptionState& exceptionState)
{
    if (options.id().isEmpty() && !options.control()) {
        exceptionState.throwDOMException(NotSupportedError, "Both id and control are null.");
        return;
    }

    Path hitRegionPath = options.hasPath() ? options.path()->path() : m_path;

    SkCanvas* c = drawingCanvas();

    if (hitRegionPath.isEmpty() || !c || !state().isTransformInvertible()
        || !c->getClipDeviceBounds(0)) {
        exceptionState.throwDOMException(NotSupportedError, "The specified path has no pixels.");
        return;
    }

    hitRegionPath.transform(state().transform());

    if (state().hasClip()) {
        hitRegionPath = state().intersectPathWithClip(hitRegionPath.skPath());
        if (hitRegionPath.isEmpty())
            exceptionState.throwDOMException(NotSupportedError, "The specified path has no pixels.");
    }

    if (!m_hitRegionManager)
        m_hitRegionManager = HitRegionManager::create();

    // Remove previous region (with id or control)
    m_hitRegionManager->removeHitRegionById(options.id());
    m_hitRegionManager->removeHitRegionByControl(options.control().get());

    RefPtrWillBeRawPtr<HitRegion> hitRegion = HitRegion::create(hitRegionPath, options);
    Element* element = hitRegion->control();
    if (element && element->isDescendantOf(canvas()))
        updateElementAccessibility(hitRegion->path(), hitRegion->control());
    m_hitRegionManager->addHitRegion(hitRegion.release());
}

void CanvasRenderingContext2D::removeHitRegion(const String& id)
{
    if (m_hitRegionManager)
        m_hitRegionManager->removeHitRegionById(id);
}

void CanvasRenderingContext2D::clearHitRegions()
{
    if (m_hitRegionManager)
        m_hitRegionManager->removeAllHitRegions();
}

HitRegion* CanvasRenderingContext2D::hitRegionAtPoint(const FloatPoint& point)
{
    if (m_hitRegionManager)
        return m_hitRegionManager->getHitRegionAtPoint(point);

    return nullptr;
}

unsigned CanvasRenderingContext2D::hitRegionsCount() const
{
    if (m_hitRegionManager)
        return m_hitRegionManager->getHitRegionsCount();

    return 0;
}

void CanvasRenderingContext2D::checkOverdraw(const SkRect& rect, const SkPaint* paint, CanvasRenderingContext2DState::ImageType imageType, DrawType drawType)
{
    SkCanvas* c = drawingCanvas();
    if (!c || !canvas()->buffer()->isRecording())
        return;

    SkRect deviceRect;
    if (drawType == UntransformedUnclippedFill) {
        deviceRect = rect;
    } else {
        ASSERT(drawType == ClipFill);
        if (state().hasComplexClip())
            return;

        SkIRect skIBounds;
        if (!c->getClipDeviceBounds(&skIBounds))
            return;
        deviceRect = SkRect::Make(skIBounds);
    }

    const SkImageInfo& imageInfo = c->imageInfo();
    if (!deviceRect.contains(SkRect::MakeWH(imageInfo.width(), imageInfo.height())))
        return;

    bool isSourceOver = true;
    unsigned alpha = 0xFF;
    if (paint) {
        if (paint->getLooper() || paint->getImageFilter() || paint->getMaskFilter())
            return;

        SkXfermode* xfermode = paint->getXfermode();
        if (xfermode) {
            SkXfermode::Mode mode;
            if (xfermode->asMode(&mode)) {
                isSourceOver = mode == SkXfermode::kSrcOver_Mode;
                if (!isSourceOver && mode != SkXfermode::kSrc_Mode && mode != SkXfermode::kClear_Mode)
                    return; // The code below only knows how to handle Src, SrcOver, and Clear
            } else {
                // unknown xfermode
                ASSERT_NOT_REACHED();
                return;
            }
        }

        alpha = paint->getAlpha();

        if (isSourceOver && imageType == CanvasRenderingContext2DState::NoImage) {
            SkShader* shader = paint->getShader();
            if (shader) {
                if (shader->isOpaque() && alpha == 0xFF)
                    canvas()->buffer()->willOverwriteCanvas();
                return;
            }
        }
    }

    if (isSourceOver) {
        // With source over, we need to certify that alpha == 0xFF for all pixels
        if (imageType == CanvasRenderingContext2DState::NonOpaqueImage)
            return;
        if (alpha < 0xFF)
            return;
    }

    canvas()->buffer()->willOverwriteCanvas();
}

} // namespace blink
