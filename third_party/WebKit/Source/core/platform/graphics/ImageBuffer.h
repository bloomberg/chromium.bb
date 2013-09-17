/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#ifndef ImageBuffer_h
#define ImageBuffer_h

#include "core/platform/graphics/ColorSpace.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/GraphicsTypes.h"
#include "core/platform/graphics/GraphicsTypes3D.h"
#include "core/platform/graphics/IntSize.h"
#include "core/platform/graphics/chromium/Canvas2DLayerBridge.h"
#include "core/platform/graphics/transforms/AffineTransform.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Uint8ClampedArray.h"
#include "wtf/Vector.h"

class SkCanvas;

namespace WebKit { class WebLayer; }

namespace WebCore {

class Image;
class ImageData;
class IntPoint;
class IntRect;
class GraphicsContext3D;

enum Multiply {
    Premultiplied,
    Unmultiplied
};

enum RenderingMode {
    Unaccelerated,
    UnacceleratedNonPlatformBuffer, // Use plain memory allocation rather than platform API to allocate backing store.
    Accelerated
};

enum BackingStoreCopy {
    CopyBackingStore, // Guarantee subsequent draws don't affect the copy.
    DontCopyBackingStore // Subsequent draws may affect the copy.
};

enum ScaleBehavior {
    Scaled,
    Unscaled
};

enum OpacityMode {
    NonOpaque,
    Opaque,
};

class ImageBuffer {
    WTF_MAKE_NONCOPYABLE(ImageBuffer); WTF_MAKE_FAST_ALLOCATED;
public:
    // Will return a null pointer on allocation failure.
    static PassOwnPtr<ImageBuffer> create(const IntSize& size, float resolutionScale = 1, RenderingMode renderingMode = Unaccelerated, OpacityMode opacityMode = NonOpaque)
    {
        bool success = false;
        OwnPtr<ImageBuffer> buf = adoptPtr(new ImageBuffer(size, resolutionScale, renderingMode, opacityMode, success));
        if (!success)
            return nullptr;
        return buf.release();
    }

    static PassOwnPtr<ImageBuffer> createCompatibleBuffer(const IntSize&, float resolutionScale, const GraphicsContext*, bool hasAlpha);

    ~ImageBuffer();

    // The actual resolution of the backing store
    const IntSize& internalSize() const { return m_size; }
    const IntSize& logicalSize() const { return m_logicalSize; }

    GraphicsContext* context() const;

    PassRefPtr<Image> copyImage(BackingStoreCopy = CopyBackingStore, ScaleBehavior = Scaled) const;
    // Give hints on the faster copyImage Mode, return DontCopyBackingStore if it supports the DontCopyBackingStore behavior
    // or return CopyBackingStore if it doesn't.
    static BackingStoreCopy fastCopyImageMode();

    enum CoordinateSystem { LogicalCoordinateSystem, BackingStoreCoordinateSystem };

    PassRefPtr<Uint8ClampedArray> getUnmultipliedImageData(const IntRect&, CoordinateSystem = LogicalCoordinateSystem) const;
    PassRefPtr<Uint8ClampedArray> getPremultipliedImageData(const IntRect&, CoordinateSystem = LogicalCoordinateSystem) const;

    void putByteArray(Multiply multiplied, Uint8ClampedArray*, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, CoordinateSystem = LogicalCoordinateSystem);

    String toDataURL(const String& mimeType, const double* quality = 0, CoordinateSystem = LogicalCoordinateSystem) const;
    AffineTransform baseTransform() const { return AffineTransform(); }
    void transformColorSpace(ColorSpace srcColorSpace, ColorSpace dstColorSpace);
    WebKit::WebLayer* platformLayer() const;

    // FIXME: current implementations of this method have the restriction that they only work
    // with textures that are RGB or RGBA format, UNSIGNED_BYTE type and level 0, as specified in
    // Extensions3D::canUseCopyTextureCHROMIUM().
    bool copyToPlatformTexture(GraphicsContext3D&, Platform3DObject, GC3Denum, GC3Denum, GC3Dint, bool, bool);

private:
    bool isValid() const;

    void draw(GraphicsContext*, const FloatRect&, const FloatRect& = FloatRect(0, 0, -1, -1), CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal, bool useLowQualityScale = false);
    void drawPattern(GraphicsContext*, const FloatRect&, const FloatSize&, const FloatPoint&, CompositeOperator, const FloatRect&, BlendMode);
    static PassRefPtr<SkColorFilter> createColorSpaceFilter(ColorSpace srcColorSpace, ColorSpace dstColorSpace);

    friend class GraphicsContext;
    friend class GeneratedImage;
    friend class CrossfadeGeneratedImage;
    friend class GeneratorGeneratedImage;
    friend class SkiaImageFilterBuilder;

    IntSize m_size;
    IntSize m_logicalSize;
    float m_resolutionScale;
    RefPtr<SkCanvas> m_canvas;
    OwnPtr<GraphicsContext> m_context;
    Canvas2DLayerBridgePtr m_layerBridge;

    // This constructor will place its success into the given out-variable
    // so that create() knows when it should return failure.
    ImageBuffer(const IntSize&, float resolutionScale, RenderingMode, OpacityMode, bool& success);
    ImageBuffer(const IntSize&, float resolutionScale, const GraphicsContext*, bool hasAlpha, bool& success);
};

String ImageDataToDataURL(const ImageData&, const String& mimeType, const double* quality);

} // namespace WebCore

#endif // ImageBuffer_h
