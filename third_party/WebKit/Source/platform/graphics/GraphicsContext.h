/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef GraphicsContext_h
#define GraphicsContext_h

#include "platform/PlatformExport.h"
#include "platform/fonts/Font.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "platform/graphics/DashArray.h"
#include "platform/graphics/DrawLooperBuilder.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/graphics/GraphicsContextAnnotation.h"
#include "platform/graphics/GraphicsContextState.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "wtf/FastAllocBase.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"

class SkBitmap;
class SkImage;
class SkPaint;
class SkPath;
class SkPicture;
class SkRRect;
class SkTextBlob;
struct SkImageInfo;
struct SkRect;

namespace blink {

class ClipRecorderStack;
class DisplayItemList;
class ImageBuffer;
class KURL;

class PLATFORM_EXPORT GraphicsContext {
    WTF_MAKE_NONCOPYABLE(GraphicsContext); WTF_MAKE_FAST_ALLOCATED;
public:
    enum AccessMode {
        ReadOnly,
        ReadWrite
    };

    enum DisabledMode {
        NothingDisabled = 0, // Run as normal.
        FullyDisabled = 1 // Do absolutely minimal work to remove the cost of the context from performance tests.
    };

    // A 0 canvas is allowed, but in such cases the context must only have canvas
    // related commands called when within a beginRecording/endRecording block.
    // Furthermore, save/restore calls must be balanced any time the canvas is 0.
    explicit GraphicsContext(SkCanvas*, DisplayItemList*, DisabledMode = NothingDisabled);

    ~GraphicsContext();

    SkCanvas* canvas() { return m_canvas; }
    const SkCanvas* canvas() const { return m_canvas; }

    DisplayItemList* displayItemList() { return m_displayItemList; }

    ClipRecorderStack* clipRecorderStack() const { return m_clipRecorderStack; }
    void setClipRecorderStack(ClipRecorderStack* clipRecorderStack) { m_clipRecorderStack = clipRecorderStack; }

    void resetCanvas(SkCanvas*);

    bool contextDisabled() const { return m_disabledState; }

    // ---------- State management methods -----------------
    void save();
    void restore();

#if ENABLE(ASSERT)
    unsigned saveCount() const;
    void disableDestructionChecks() { m_disableDestructionChecks = true; }
#endif

    bool hasStroke() const { return strokeStyle() != NoStroke && strokeThickness() > 0; }

    float strokeThickness() const { return immutableState()->strokeData().thickness(); }
    void setStrokeThickness(float thickness) { mutableState()->setStrokeThickness(thickness); }

    StrokeStyle strokeStyle() const { return immutableState()->strokeData().style(); }
    void setStrokeStyle(StrokeStyle style) { mutableState()->setStrokeStyle(style); }

    Color strokeColor() const { return immutableState()->strokeColor(); }
    void setStrokeColor(const Color& color) { mutableState()->setStrokeColor(color); }
    SkColor effectiveStrokeColor() const { return immutableState()->effectiveStrokeColor(); }

    Pattern* strokePattern() const { return immutableState()->strokePattern(); }
    void setStrokePattern(PassRefPtr<Pattern>, float alpha = 1);

    Gradient* strokeGradient() const { return immutableState()->strokeGradient(); }
    void setStrokeGradient(PassRefPtr<Gradient>, float alpha = 1);

    void setLineCap(LineCap cap) { mutableState()->setLineCap(cap); }
    void setLineDash(const DashArray& dashes, float dashOffset) { mutableState()->setLineDash(dashes, dashOffset); }
    void setLineJoin(LineJoin join) { mutableState()->setLineJoin(join); }
    void setMiterLimit(float limit) { mutableState()->setMiterLimit(limit); }

    WindRule fillRule() const { return immutableState()->fillRule(); }
    void setFillRule(WindRule fillRule) { mutableState()->setFillRule(fillRule); }

    Color fillColor() const { return immutableState()->fillColor(); }
    void setFillColor(const Color& color) { mutableState()->setFillColor(color); }
    SkColor effectiveFillColor() const { return immutableState()->effectiveFillColor(); }

    void setFillPattern(PassRefPtr<Pattern>, float alpha = 1);
    Pattern* fillPattern() const { return immutableState()->fillPattern(); }

    void setFillGradient(PassRefPtr<Gradient>, float alpha = 1);
    Gradient* fillGradient() const { return immutableState()->fillGradient(); }

    SkDrawLooper* drawLooper() const { return immutableState()->drawLooper(); }

    SkMatrix getTotalMatrix() const;

    void setShouldAntialias(bool antialias) { mutableState()->setShouldAntialias(antialias); }
    bool shouldAntialias() const { return immutableState()->shouldAntialias(); }

    // Disable the anti-aliasing optimization for scales/multiple-of-90-degrees
    // rotations of thin ("hairline") images.
    // Note: This will only be reliable when the device pixel scale/ratio is
    // fixed (e.g. when drawing to context backed by an ImageBuffer).
    void disableAntialiasingOptimizationForHairlineImages() { ASSERT(!isRecording()); m_antialiasHairlineImages = true; }
    bool shouldAntialiasHairlineImages() const { return m_antialiasHairlineImages; }

    void setShouldClampToSourceRect(bool clampToSourceRect) { mutableState()->setShouldClampToSourceRect(clampToSourceRect); }
    bool shouldClampToSourceRect() const { return immutableState()->shouldClampToSourceRect(); }

    void setTextDrawingMode(TextDrawingModeFlags mode) { mutableState()->setTextDrawingMode(mode); }
    TextDrawingModeFlags textDrawingMode() const { return immutableState()->textDrawingMode(); }

    void setAlphaAsFloat(float alpha) { mutableState()->setAlphaAsFloat(alpha);}
    int getNormalizedAlpha() const
    {
        int alpha = immutableState()->alpha();
        return alpha > 255 ? 255 : alpha;
    }

    void setImageInterpolationQuality(InterpolationQuality quality) { mutableState()->setInterpolationQuality(quality); }
    InterpolationQuality imageInterpolationQuality() const { return immutableState()->interpolationQuality(); }

    void setCompositeOperation(SkXfermode::Mode);
    SkXfermode::Mode compositeOperation() const;
    // TODO(dshwang): remove these method. crbug.com/425656
    CompositeOperator compositeOperationDeprecated() const;

    // Specify the device scale factor which may change the way document markers
    // and fonts are rendered.
    void setDeviceScaleFactor(float factor) { m_deviceScaleFactor = factor; }
    float deviceScaleFactor() const { return m_deviceScaleFactor; }

    // Returns if the context is a printing context instead of a display
    // context. Bitmap shouldn't be resampled when printing to keep the best
    // possible quality.
    bool printing() const { return m_printing; }
    void setPrinting(bool printing) { m_printing = printing; }

    bool isAccelerated() const { return m_accelerated; }
    void setAccelerated(bool accelerated) { m_accelerated = accelerated; }

    // The text region is empty until tracking is turned on.
    // It is never clerared by the context.
    void setTrackTextRegion(bool track) { m_trackTextRegion = track; }
    const SkRect& textRegion() const { return m_textRegion; }

    AnnotationModeFlags annotationMode() const { return m_annotationMode; }
    void setAnnotationMode(const AnnotationModeFlags mode) { m_annotationMode = mode; }

    SkColorFilter* colorFilter() const;
    void setColorFilter(ColorFilter);
    // ---------- End state management methods -----------------

    // Get the contents of the image buffer
    bool readPixels(const SkImageInfo&, void* pixels, size_t rowBytes, int x, int y);

    // Get the current fill style.
    const SkPaint& fillPaint() const { return immutableState()->fillPaint(); }

    // Get the current stroke style.
    const SkPaint& strokePaint() const { return immutableState()->strokePaint(); }

    // These draw methods will do both stroking and filling.
    // FIXME: ...except drawRect(), which fills properly but always strokes
    // using a 1-pixel stroke inset from the rect borders (of the correct
    // stroke color).
    void drawRect(const IntRect&);
    void drawLine(const IntPoint&, const IntPoint&);

    void fillPolygon(size_t numPoints, const FloatPoint*, const Color&, bool shouldAntialias);

    void fillPath(const Path&);
    void strokePath(const Path&);

    void fillEllipse(const FloatRect&);
    void strokeEllipse(const FloatRect&);

    void fillRect(const FloatRect&);
    void fillRect(const FloatRect&, const Color&);
    void fillRect(const FloatRect&, const Color&, SkXfermode::Mode);
    void fillRoundedRect(const FloatRect&, const FloatSize& topLeft, const FloatSize& topRight, const FloatSize& bottomLeft, const FloatSize& bottomRight, const Color&);
    void fillRoundedRect(const FloatRoundedRect&, const Color&);

    void clearRect(const FloatRect&);

    void strokeRect(const FloatRect&);
    void strokeRect(const FloatRect&, float lineWidth);

    void fillBetweenRoundedRects(const FloatRect&, const FloatSize& outerTopLeft, const FloatSize& outerTopRight, const FloatSize& outerBottomLeft, const FloatSize& outerBottomRight,
        const FloatRect&, const FloatSize& innerTopLeft, const FloatSize& innerTopRight, const FloatSize& innerBottomLeft, const FloatSize& innerBottomRight, const Color&);
    void fillBetweenRoundedRects(const FloatRoundedRect&, const FloatRoundedRect&, const Color&);

    void drawPicture(const SkPicture*);
    void compositePicture(SkPicture*, const FloatRect& dest, const FloatRect& src, SkXfermode::Mode);

    void drawImage(Image*, const IntPoint&, SkXfermode::Mode = SkXfermode::kSrcOver_Mode, RespectImageOrientationEnum = DoNotRespectImageOrientation);
    void drawImage(Image*, const IntRect&, SkXfermode::Mode = SkXfermode::kSrcOver_Mode, RespectImageOrientationEnum = DoNotRespectImageOrientation);
    void drawImage(Image*, const FloatRect& destRect);
    void drawImage(Image*, const FloatRect& destRect, const FloatRect& srcRect, SkXfermode::Mode = SkXfermode::kSrcOver_Mode, RespectImageOrientationEnum = DoNotRespectImageOrientation);

    void drawTiledImage(Image*, const IntRect& destRect, const IntPoint& srcPoint, const IntSize& tileSize,
        SkXfermode::Mode = SkXfermode::kSrcOver_Mode, const IntSize& repeatSpacing = IntSize());
    void drawTiledImage(Image*, const IntRect& destRect, const IntRect& srcRect,
        const FloatSize& tileScaleFactor, Image::TileRule hRule = Image::StretchTile, Image::TileRule vRule = Image::StretchTile,
        SkXfermode::Mode = SkXfermode::kSrcOver_Mode);

    void drawImageBuffer(ImageBuffer*, const FloatRect& destRect, const FloatRect* srcRect = 0, SkXfermode::Mode = SkXfermode::kSrcOver_Mode);

    // These methods write to the canvas.
    // Also drawLine(const IntPoint& point1, const IntPoint& point2) and fillRoundedRect
    void writePixels(const SkImageInfo&, const void* pixels, size_t rowBytes, int x, int y);
    void drawBitmapRect(const SkBitmap&, const SkRect*, const SkRect&, const SkPaint* = 0);
    void drawImage(const SkImage*, SkScalar, SkScalar, const SkPaint* = 0);
    void drawImageRect(const SkImage*, const SkRect*, const SkRect&, const SkPaint* = 0);
    void drawOval(const SkRect&, const SkPaint&);
    void drawPath(const SkPath&, const SkPaint&);
    void drawRect(const SkRect&, const SkPaint&);
    void drawPosText(const void* text, size_t byteLength, const SkPoint pos[], const SkRect& textRect, const SkPaint&);
    void drawPosTextH(const void* text, size_t byteLength, const SkScalar xpos[], SkScalar constY, const SkRect& textRect, const SkPaint&);
    void drawTextBlob(const SkTextBlob*, const SkPoint& origin, const SkPaint&);

    void clip(const IntRect& rect) { clipRect(rect); }
    void clip(const FloatRect& rect) { clipRect(rect); }
    void clipRoundedRect(const FloatRoundedRect&, SkRegion::Op = SkRegion::kIntersect_Op);
    void clipOut(const IntRect& rect) { clipRect(rect, NotAntiAliased, SkRegion::kDifference_Op); }
    void clipOut(const FloatRect& rect) { clipRect(rect, NotAntiAliased, SkRegion::kDifference_Op); }
    void clipOut(const Path&);
    void clipOutRoundedRect(const FloatRoundedRect&);
    void clipPath(const Path&, WindRule = RULE_EVENODD, AntiAliasingMode = AntiAliased);
    void clipPath(const SkPath&, AntiAliasingMode = NotAntiAliased, SkRegion::Op = SkRegion::kIntersect_Op);
    void clipPolygon(size_t numPoints, const FloatPoint*, bool antialias);
    void clipRect(const SkRect&, AntiAliasingMode = NotAntiAliased, SkRegion::Op = SkRegion::kIntersect_Op);

    void drawText(const Font&, const TextRunPaintInfo&, const FloatPoint&);
    void drawEmphasisMarks(const Font&, const TextRunPaintInfo&, const AtomicString& mark, const FloatPoint&);
    void drawBidiText(const Font&, const TextRunPaintInfo&, const FloatPoint&, Font::CustomFontNotReadyAction = Font::DoNotPaintIfFontNotReady);
    void drawHighlightForText(const Font&, const TextRun&, const FloatPoint&, int h, const Color& backgroundColor, int from = 0, int to = -1);

    void drawLineForText(const FloatPoint&, float width, bool printing);
    enum DocumentMarkerLineStyle {
        DocumentMarkerSpellingLineStyle,
        DocumentMarkerGrammarLineStyle
    };
    void drawLineForDocumentMarker(const FloatPoint&, float width, DocumentMarkerLineStyle);

    // beginLayer()/endLayer() behaves like save()/restore() for only CTM and clip states.
    void beginTransparencyLayer(float opacity, const FloatRect* = 0);
    // Apply SkXfermode::Mode when the layer is composited on the backdrop (i.e. endLayer()).
    // Don't change the current SkXfermode::Mode states.
    void beginLayer(float opacity, SkXfermode::Mode, const FloatRect* = 0, ColorFilter = ColorFilterNone, SkImageFilter* = 0);
    void endLayer();
    bool isDrawingToLayer() { return m_layerCount; }

    // Instead of being dispatched to the active canvas, draw commands following beginRecording()
    // are stored in a display list that can be replayed at a later time. Pass in the bounding
    // rectangle for the content in the list.
    void beginRecording(const FloatRect&, uint32_t = 0);
    PassRefPtr<const SkPicture> endRecording();

    bool hasShadow() const;
    void setShadow(const FloatSize& offset, float blur, const Color&,
        DrawLooperBuilder::ShadowTransformMode = DrawLooperBuilder::ShadowRespectsTransforms,
        DrawLooperBuilder::ShadowAlphaMode = DrawLooperBuilder::ShadowRespectsAlpha, ShadowMode = DrawShadowAndForeground);
    void clearShadow() { clearDrawLooper(); clearDropShadowImageFilter(); }

    // It is assumed that this draw looper is used only for shadows
    // (i.e. a draw looper is set if and only if there is a shadow).
    // The builder passed into this method will be destroyed.
    void setDrawLooper(PassOwnPtr<DrawLooperBuilder>);
    void clearDrawLooper();

    void drawFocusRing(const Vector<IntRect>&, int width, int offset, const Color&);
    void drawFocusRing(const Path&, int width, int offset, const Color&);

    enum Edge {
        NoEdge = 0,
        TopEdge = 1 << 1,
        RightEdge = 1 << 2,
        BottomEdge = 1 << 3,
        LeftEdge = 1 << 4
    };
    typedef unsigned Edges;
    void drawInnerShadow(const FloatRoundedRect&, const Color& shadowColor, const IntSize shadowOffset, int shadowBlur, int shadowSpread, Edges clippedEdges = NoEdge);

    // ---------- Transformation methods -----------------
    // Note that the getCTM method returns only the current transform from Blink's perspective,
    // which is not the final transform used to place content on screen. It cannot be relied upon
    // for testing where a point will appear on screen or how large it will be.
    AffineTransform getCTM() const;
    void concatCTM(const AffineTransform&);
    void setCTM(const AffineTransform&);
    void setMatrix(const SkMatrix&);

    void scale(float x, float y);
    void rotate(float angleInRadians);
    void translate(float x, float y);
    // ---------- End transformation methods -----------------

    // URL drawing
    void setURLForRect(const KURL&, const IntRect&);
    void setURLFragmentForRect(const String& name, const IntRect&);
    void addURLTargetAtPoint(const String& name, const IntPoint&);

    static void adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth, StrokeStyle);

    void beginAnnotation(const AnnotationList&);
    void endAnnotation();

    // This method can potentially push saves onto the canvas. It returns the initial save count,
    // and should be balanced with a call to context->canvas()->restoreToCount(initialSaveCount).
    WARN_UNUSED_RETURN int preparePaintForDrawRectToRect(
        SkPaint*,
        const SkRect& srcRect,
        const SkRect& destRect,
        SkXfermode::Mode,
        bool isBitmapWithAlpha,
        bool isLazyDecoded = false,
        bool isDataComplete = true) const;

    static int focusRingOutsetExtent(int offset, int width)
    {
        return focusRingOutset(offset) + (focusRingWidth(width) + 1) / 2;
    }

    // public decl needed for OwnPtr wrapper.
    class RecordingState;

#if ENABLE(ASSERT)
    void setInDrawingRecorder(bool);
#endif

    static PassRefPtr<SkColorFilter> WebCoreColorFilterToSkiaColorFilter(ColorFilter);

private:
    const GraphicsContextState* immutableState() const { return m_paintState; }

    GraphicsContextState* mutableState()
    {
        realizePaintSave();
        return m_paintState;
    }

    static void setPathFromPoints(SkPath*, size_t, const FloatPoint*);
    static void setRadii(SkVector*, FloatSize, FloatSize, FloatSize, FloatSize);

#if OS(MACOSX)
    static inline int focusRingOutset(int offset) { return offset + 2; }
    static inline int focusRingWidth(int width) { return width; }
#else
    static inline int focusRingOutset(int offset) { return 0; }
    static inline int focusRingWidth(int width) { return 1; }
    static SkPMColor lineColors(int);
    static SkPMColor antiColors1(int);
    static SkPMColor antiColors2(int);
    static void draw1xMarker(SkBitmap*, int);
    static void draw2xMarker(SkBitmap*, int);
#endif

    void saveLayer(const SkRect* bounds, const SkPaint*);
    void restoreLayer();

    // Helpers for drawing a focus ring (drawFocusRing)
    float prepareFocusRingPaint(SkPaint&, const Color&, int width) const;
    void drawFocusRingPath(const SkPath&, const Color&, int width);
    void drawFocusRingRect(const SkRect&, const Color&, int width);

    // SkCanvas wrappers.
    void clipRRect(const SkRRect&, AntiAliasingMode = NotAntiAliased, SkRegion::Op = SkRegion::kIntersect_Op);
    void concat(const SkMatrix&);
    void drawRRect(const SkRRect&, const SkPaint&);

    void setDropShadowImageFilter(PassRefPtr<SkImageFilter>);
    void clearDropShadowImageFilter();
    SkImageFilter* dropShadowImageFilter() const { return immutableState()->dropShadowImageFilter(); }

    // Apply deferred paint state saves
    void realizePaintSave()
    {
        if (contextDisabled())
            return;

        if (m_paintState->saveCount()) {
            m_paintState->decrementSaveCount();
            ++m_paintStateIndex;
            if (m_paintStateStack.size() == m_paintStateIndex) {
                m_paintStateStack.append(GraphicsContextState::createAndCopy(*m_paintState));
                m_paintState = m_paintStateStack[m_paintStateIndex].get();
            } else {
                GraphicsContextState* priorPaintState = m_paintState;
                m_paintState = m_paintStateStack[m_paintStateIndex].get();
                m_paintState->copy(*priorPaintState);
            }
        }
    }

    void didDrawTextInRect(const SkRect& textRect);

    void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect& roundedHoleRect, const Color&);

    bool isRecording() const;

    // null indicates painting is contextDisabled. Never delete this object.
    SkCanvas* m_canvas;

    // This being null indicates not to paint into a DisplayItemList, and instead directly into the canvas.
    DisplayItemList* m_displayItemList;

    ClipRecorderStack* m_clipRecorderStack;

    // Paint states stack. Enables local drawing state change with save()/restore() calls.
    // This state controls the appearance of drawn content.
    // We do not delete from this stack to avoid memory churn.
    Vector<OwnPtr<GraphicsContextState>> m_paintStateStack;
    // Current index on the stack. May not be the last thing on the stack.
    unsigned m_paintStateIndex;
    // Raw pointer to the current state.
    GraphicsContextState* m_paintState;

    AnnotationModeFlags m_annotationMode;

    Vector<OwnPtr<RecordingState>> m_recordingStateStack;

    unsigned m_layerCount;

#if ENABLE(ASSERT)
    unsigned m_annotationCount;
    bool m_disableDestructionChecks;
    bool m_inDrawingRecorder;
#endif

    // Tracks the region where text is painted via the GraphicsContext.
    SkRect m_textRegion;

    const DisabledMode m_disabledState;

    float m_deviceScaleFactor;

    unsigned m_trackTextRegion : 1;

    unsigned m_accelerated : 1;
    unsigned m_printing : 1;
    unsigned m_antialiasHairlineImages : 1;
};

} // namespace blink

#endif // GraphicsContext_h
