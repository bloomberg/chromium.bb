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

#include "core/platform/graphics/ColorSpace.h"
#include "core/platform/graphics/DashArray.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/Font.h"
#include "core/platform/graphics/Gradient.h"
#include "core/platform/graphics/Image.h"
#include "core/platform/graphics/ImageOrientation.h"
#include "core/platform/graphics/Path.h"
#include "core/platform/graphics/Pattern.h"
#include "core/platform/graphics/skia/OpaqueRegionSkia.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/effects/SkCornerPathEffect.h"

#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {
class PlatformContextSkia;
}
typedef WebCore::PlatformContextSkia PlatformGraphicsContext;

namespace WebCore {

    const int cMisspellingLineThickness = 3;
    const int cMisspellingLinePatternWidth = 4;
    const int cMisspellingLinePatternGapWidth = 1;

    class AffineTransform;
    class DrawingBuffer;
    class Generator;
    class ImageBuffer;
    class IntRect;
    class RoundedRect;
    class KURL;
    class GraphicsContext3D;
    class TextRun;
    class TransformationMatrix;

    enum TextDrawingMode {
        TextModeFill      = 1 << 0,
        TextModeStroke    = 1 << 1,
    };
    typedef unsigned TextDrawingModeFlags;

    enum StrokeStyle {
        NoStroke,
        SolidStroke,
        DottedStroke,
        DashedStroke,
#if ENABLE(CSS3_TEXT)
        DoubleStroke,
        WavyStroke,
#endif // CSS3_TEXT
    };

    enum InterpolationQuality {
        InterpolationDefault,
        InterpolationNone,
        InterpolationLow,
        InterpolationMedium,
        InterpolationHigh
    };

    struct GraphicsContextState {
        GraphicsContextState()
            : strokeThickness(0)
            , shadowBlur(0)
            , textDrawingMode(TextModeFill)
            , strokeColor(Color::black)
            , fillColor(Color::black)
            , strokeStyle(SolidStroke)
            , fillRule(RULE_NONZERO)
            , strokeColorSpace(ColorSpaceDeviceRGB)
            , fillColorSpace(ColorSpaceDeviceRGB)
            , shadowColorSpace(ColorSpaceDeviceRGB)
            , compositeOperator(CompositeSourceOver)
            , blendMode(BlendModeNormal)
            , shouldAntialias(true)
            , shouldSmoothFonts(true)
            , shouldSubpixelQuantizeFonts(true)
            , paintingDisabled(false)
            , shadowsIgnoreTransforms(false)
        {
        }

        RefPtr<Gradient> strokeGradient;
        RefPtr<Pattern> strokePattern;
        
        RefPtr<Gradient> fillGradient;
        RefPtr<Pattern> fillPattern;

        FloatSize shadowOffset;

        float strokeThickness;
        float shadowBlur;

        TextDrawingModeFlags textDrawingMode;

        Color strokeColor;
        Color fillColor;
        Color shadowColor;

        StrokeStyle strokeStyle;
        WindRule fillRule;

        ColorSpace strokeColorSpace;
        ColorSpace fillColorSpace;
        ColorSpace shadowColorSpace;

        CompositeOperator compositeOperator;
        BlendMode blendMode;

        bool shouldAntialias : 1;
        bool shouldSmoothFonts : 1;
        bool shouldSubpixelQuantizeFonts : 1;
        bool paintingDisabled : 1;
        bool shadowsIgnoreTransforms : 1;
    };

    class GraphicsContext {
        WTF_MAKE_NONCOPYABLE(GraphicsContext); WTF_MAKE_FAST_ALLOCATED;
    public:
        explicit GraphicsContext(SkCanvas*);
        ~GraphicsContext();

        PlatformGraphicsContext* platformContext() const;

        float strokeThickness() const;
        void setStrokeThickness(float);
        StrokeStyle strokeStyle() const;
        void setStrokeStyle(StrokeStyle);
        Color strokeColor() const;
        ColorSpace strokeColorSpace() const;
        void setStrokeColor(const Color&, ColorSpace);

        void setStrokePattern(PassRefPtr<Pattern>);
        Pattern* strokePattern() const;

        void setStrokeGradient(PassRefPtr<Gradient>);
        Gradient* strokeGradient() const;

        WindRule fillRule() const;
        void setFillRule(WindRule);
        Color fillColor() const;
        ColorSpace fillColorSpace() const;
        void setFillColor(const Color&, ColorSpace);

        void setFillPattern(PassRefPtr<Pattern>);
        Pattern* fillPattern() const;

        void setFillGradient(PassRefPtr<Gradient>);
        Gradient* fillGradient() const;

        void setShadowsIgnoreTransforms(bool);
        bool shadowsIgnoreTransforms() const;

        void setShouldAntialias(bool);
        bool shouldAntialias() const;

        void setShouldSmoothFonts(bool);
        bool shouldSmoothFonts() const;

        // Normally CG enables subpixel-quantization because it improves the performance of aligning glyphs.
        // In some cases we have to disable to to ensure a high-quality output of the glyphs.
        void setShouldSubpixelQuantizeFonts(bool);
        bool shouldSubpixelQuantizeFonts() const;

        // Change the way document markers are rendered.
        // Any deviceScaleFactor higher than 1.5 is enough to justify setting this flag.
        void setUseHighResMarkers(bool isHighRes) { m_useHighResMarker = isHighRes; }

        // If true we are (most likely) rendering to a web page and the
        // canvas has been prepared with an opaque background. If false,
        // the canvas may havbe transparency (as is the case when rendering
        // to a canvas object).
        void setCertainlyOpaque(bool isOpaque) { m_isCertainlyOpaque = isOpaque; }
        bool isCertainlyOpaque() const { return m_isCertainlyOpaque; }

        // Returns if the context is a printing context instead of a display
        // context. Bitmap shouldn't be resampled when printing to keep the best
        // possible quality.
        bool printing() const { return m_printing; }
        void setPrinting(bool printing) { m_printing = printing; }

        const GraphicsContextState& state() const;

        bool isAccelerated() const { return m_accelerated; }
        void setAccelerated(bool accelerated) { m_accelerated = accelerated; }

        // The opaque region is empty until tracking is turned on.
        // It is never clerared by the context.
        void setTrackOpaqueRegion(bool track) { m_trackOpaqueRegion = track; }
        const OpaqueRegionSkia& opaqueRegion() const { return m_opaqueRegion; }

        void setImageInterpolationQuality(InterpolationQuality);
        InterpolationQuality imageInterpolationQuality() const;

        void save();
        void restore();

        void saveLayer(const SkRect* bounds, const SkPaint*, SkCanvas::SaveFlags = SkCanvas::kARGB_ClipLayer_SaveFlag);
        void restoreLayer();

        // These draw methods will do both stroking and filling.
        // FIXME: ...except drawRect(), which fills properly but always strokes
        // using a 1-pixel stroke inset from the rect borders (of the correct
        // stroke color).
        void drawRect(const IntRect&);
        void drawLine(const IntPoint&, const IntPoint&);
        void drawEllipse(const IntRect&);
        void drawConvexPolygon(size_t numPoints, const FloatPoint*, bool shouldAntialias = false);

        void fillPath(const Path&);
        void strokePath(const Path&);

        void fillEllipse(const FloatRect&);
        void strokeEllipse(const FloatRect&);

        void fillRect(const FloatRect&);
        void fillRect(const FloatRect&, const Color&, ColorSpace);
        void fillRect(const FloatRect&, Generator&);
        void fillRect(const FloatRect&, const Color&, ColorSpace, CompositeOperator);
        void fillRoundedRect(const IntRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color&, ColorSpace);
        void fillRoundedRect(const RoundedRect&, const Color&, ColorSpace);
        void fillRectWithRoundedHole(const IntRect&, const RoundedRect& roundedHoleRect, const Color&, ColorSpace);

        void clearRect(const FloatRect&);

        void strokeRect(const FloatRect&, float lineWidth);

        void drawImage(Image*, ColorSpace styleColorSpace, const IntPoint&, CompositeOperator = CompositeSourceOver, RespectImageOrientationEnum = DoNotRespectImageOrientation);
        void drawImage(Image*, ColorSpace styleColorSpace, const IntRect&, CompositeOperator = CompositeSourceOver, RespectImageOrientationEnum = DoNotRespectImageOrientation, bool useLowQualityScale = false);
        void drawImage(Image*, ColorSpace styleColorSpace, const IntPoint& destPoint, const IntRect& srcRect, CompositeOperator = CompositeSourceOver, RespectImageOrientationEnum = DoNotRespectImageOrientation);
        void drawImage(Image*, ColorSpace styleColorSpace, const FloatRect& destRect);
        void drawImage(Image*, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator = CompositeSourceOver, RespectImageOrientationEnum = DoNotRespectImageOrientation, bool useLowQualityScale = false);
        void drawImage(Image*, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator, BlendMode, RespectImageOrientationEnum = DoNotRespectImageOrientation, bool useLowQualityScale = false);
        
        void drawTiledImage(Image*, ColorSpace styleColorSpace, const IntRect& destRect, const IntPoint& srcPoint, const IntSize& tileSize,
            CompositeOperator = CompositeSourceOver, bool useLowQualityScale = false, BlendMode = BlendModeNormal);
        void drawTiledImage(Image*, ColorSpace styleColorSpace, const IntRect& destRect, const IntRect& srcRect,
                            const FloatSize& tileScaleFactor, Image::TileRule hRule = Image::StretchTile, Image::TileRule vRule = Image::StretchTile,
                            CompositeOperator = CompositeSourceOver, bool useLowQualityScale = false);

        void drawImageBuffer(ImageBuffer*, ColorSpace styleColorSpace, const IntPoint&, CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal);
        void drawImageBuffer(ImageBuffer*, ColorSpace styleColorSpace, const IntRect&, CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal, bool useLowQualityScale = false);
        void drawImageBuffer(ImageBuffer*, ColorSpace styleColorSpace, const IntPoint& destPoint, const IntRect& srcRect, CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal);
        void drawImageBuffer(ImageBuffer*, ColorSpace styleColorSpace, const IntRect& destRect, const IntRect& srcRect, CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal, bool useLowQualityScale = false);
        void drawImageBuffer(ImageBuffer*, ColorSpace styleColorSpace, const FloatRect& destRect);
        void drawImageBuffer(ImageBuffer*, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal, bool useLowQualityScale = false);

        // These methods write to the canvas and modify the opaque region, if tracked.
        // Also drawLine(const IntPoint& point1, const IntPoint& point2) and fillRoundedRect
        void writePixels(const SkBitmap&, int x, int y, SkCanvas::Config8888 = SkCanvas::kNative_Premul_Config8888);
        void drawBitmap(const SkBitmap&, SkScalar, SkScalar, const SkPaint* = 0);
        void drawBitmapRect(const SkBitmap&, const SkIRect*, const SkRect&, const SkPaint* = 0);
        void drawOval(const SkRect&, const SkPaint&);
        void drawPath(const SkPath&, const SkPaint&);
        // After drawing directly to the context's canvas, use this function to notify the context so
        // it can track the opaque region.
        // FIXME: this is still needed only because ImageSkia::paintSkBitmap() may need to notify for a
        //        smaller rect than the one drawn to, due to its clipping logic.
        void didDrawRect(const SkRect&, const SkPaint&, const SkBitmap* = 0);
        void drawRect(const SkRect&, const SkPaint&);
        void drawPosText(const void* text, size_t byteLength, const SkPoint pos[], const SkPaint&);
        void drawPosTextH(const void* text, size_t byteLength, const SkScalar xpos[], SkScalar constY, const SkPaint&);
        void drawTextOnPath(const void* text, size_t byteLength, const SkPath&, const SkMatrix*, const SkPaint&);

        void clip(const IntRect&);
        void clip(const FloatRect&);
        void clipRoundedRect(const RoundedRect&);
        void clipOut(const IntRect&);
        void clipOutRoundedRect(const RoundedRect&);
        void clipPath(const Path&, WindRule);
        void clipConvexPolygon(size_t numPoints, const FloatPoint*, bool antialias = true);
        void clipToImageBuffer(ImageBuffer*, const FloatRect&);
        
        TextDrawingModeFlags textDrawingMode() const;
        void setTextDrawingMode(TextDrawingModeFlags);

        void drawText(const Font&, const TextRun&, const FloatPoint&, int from = 0, int to = -1);
        void drawEmphasisMarks(const Font&, const TextRun& , const AtomicString& mark, const FloatPoint&, int from = 0, int to = -1);
        void drawBidiText(const Font&, const TextRun&, const FloatPoint&, Font::CustomFontNotReadyAction = Font::DoNotPaintIfFontNotReady);
        void drawHighlightForText(const Font&, const TextRun&, const FloatPoint&, int h, const Color& backgroundColor, ColorSpace, int from = 0, int to = -1);

        void drawLineForText(const FloatPoint&, float width, bool printing);
        enum DocumentMarkerLineStyle {
            DocumentMarkerSpellingLineStyle,
            DocumentMarkerGrammarLineStyle,
            DocumentMarkerAutocorrectionReplacementLineStyle,
            DocumentMarkerDictationAlternativesLineStyle
        };
        void drawLineForDocumentMarker(const FloatPoint&, float width, DocumentMarkerLineStyle);

        bool paintingDisabled() const;
        void setPaintingDisabled(bool);

        bool updatingControlTints() const;
        void setUpdatingControlTints(bool);

        void beginTransparencyLayer(float opacity);
        void endTransparencyLayer();
        bool isInTransparencyLayer() const;
        // Begins a layer that is clipped to the image |imageBuffer| at the location
        // |rect|. This layer is implicitly restored when the next restore is invoked.
        // NOTE: |imageBuffer| may be deleted before the |restore| is invoked.
        void beginLayerClippedToImage(const FloatRect&, const ImageBuffer*);

        bool hasShadow() const;
        void setShadow(const FloatSize&, float blur, const Color&, ColorSpace);
        // Legacy shadow blur radius is used for canvas, and -webkit-box-shadow.
        // It has different treatment of radii > 8px.
        void setLegacyShadow(const FloatSize&, float blur, const Color&, ColorSpace);

        bool getShadow(FloatSize&, float&, Color&, ColorSpace&) const;
        void clearShadow();

        void drawFocusRing(const Vector<IntRect>&, int width, int offset, const Color&);
        void drawFocusRing(const Path&, int width, int offset, const Color&);

        void setLineCap(LineCap);
        void setLineDash(const DashArray&, float dashOffset);
        void setLineJoin(LineJoin);
        void setMiterLimit(float);

        void setAlpha(float);

        void setCompositeOperation(CompositeOperator, BlendMode = BlendModeNormal);
        CompositeOperator compositeOperation() const;
        BlendMode blendModeOperation() const;

        void clip(const Path&, WindRule = RULE_EVENODD);

        // This clip function is used only by <canvas> code. It allows
        // implementations to handle clipping on the canvas differently since
        // the discipline is different.
        void canvasClip(const Path&, WindRule = RULE_EVENODD);
        void clipOut(const Path&);

        void scale(const FloatSize&);
        void rotate(float angleInRadians);
        void translate(const FloatSize& size) { translate(size.width(), size.height()); }
        void translate(float x, float y);

        void setURLForRect(const KURL&, const IntRect&);

        void setURLFragmentForRect(const String& name, const IntRect&);
        void addURLTargetAtPoint(const String& name, const IntPoint&);

        void concatCTM(const AffineTransform&);
        void setCTM(const AffineTransform&);

        enum IncludeDeviceScale { DefinitelyIncludeDeviceScale, PossiblyIncludeDeviceScale };
        AffineTransform getCTM(IncludeDeviceScale includeScale = PossiblyIncludeDeviceScale) const;

        // Create an image buffer compatible with this context, with suitable resolution
        // for drawing into the buffer and then into this context.
        PassOwnPtr<ImageBuffer> createCompatibleBuffer(const IntSize&, bool hasAlpha = true) const;
        bool isCompatibleWithBuffer(ImageBuffer*) const;

        // This function applies the device scale factor to the context, making the context capable of
        // acting as a base-level context for a HiDPI environment.
        void applyDeviceScaleFactor(float);

        bool shouldIncludeChildWindows() const { return false; }

        static void adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth, StrokeStyle);
        bool supportsURLFragments() { return printing(); }

    private:
        static bool supportsTransparencyLayers();
        static void addCornerArc(SkPath*, const SkRect&, const IntSize&, int);
        static void setPathFromConvexPoints(SkPath* path, size_t numPoints, const FloatPoint* points);
        void drawOuterPath(const SkPath&, SkPaint&, int);
        void drawInnerPath(const SkPath&, SkPaint&, int);
        static void setRadii(SkVector* radii, IntSize topLeft, IntSize topRight, IntSize bottomRight, IntSize bottomLeft);

#if OS(DARWIN)
        static inline int getFocusRingOutset(int offset) { return offset + 2; }
#else
        static inline int getFocusRingOutset(int offset) { return 0; }
        static const SkPMColor lineColors(int);
        static const SkPMColor antiColors1(int);
        static const SkPMColor antiColors2(int);
        static void draw1xMarker(SkBitmap*, int);
        static void draw2xMarker(SkBitmap*, int);
#endif

        // Return value % max, but account for value possibly being negative.
        static int fastMod(int value, int max)
        {
            bool isNeg = false;
            if (value < 0) {
                value = -value;
                isNeg = true;
            }
            if (value >= max)
                value %= max;
            if (isNeg)
                value = -value;
            return value;
        }

        OwnPtr<PlatformContextSkia> m_data;

        GraphicsContextState m_state;
        Vector<GraphicsContextState> m_stack;
        unsigned m_transparencyCount;

        // Tracks the region painted opaque via the GraphicsContext.
        OpaqueRegionSkia m_opaqueRegion;
        bool m_trackOpaqueRegion;

        // Are we on a high DPI display? If so, spelling and grammer markers are larger.
        bool m_useHighResMarker;
        // FIXME: Make this go away: crbug.com/236892
        bool m_updatingControlTints;
        bool m_accelerated;
        bool m_isCertainlyOpaque;
        bool m_printing;
    };
} // namespace WebCore

#endif // GraphicsContext_h
