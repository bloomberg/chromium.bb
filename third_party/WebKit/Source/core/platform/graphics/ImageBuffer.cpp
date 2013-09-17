/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/platform/graphics/ImageBuffer.h"

#include "core/html/ImageData.h"
#include "core/platform/MIMETypeRegistry.h"
#include "core/platform/graphics/BitmapImage.h"
#include "core/platform/graphics/Extensions3D.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/GraphicsContext3D.h"
#include "core/platform/graphics/IntRect.h"
#include "core/platform/graphics/chromium/Canvas2DLayerBridge.h"
#include "core/platform/graphics/gpu/SharedGraphicsContext3D.h"
#include "core/platform/graphics/skia/NativeImageSkia.h"
#include "core/platform/graphics/skia/SkiaUtils.h"
#include "core/platform/image-encoders/skia/JPEGImageEncoder.h"
#include "core/platform/image-encoders/skia/PNGImageEncoder.h"
#include "core/platform/image-encoders/skia/WEBPImageEncoder.h"
#include "public/platform/Platform.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/effects/SkTableColorFilter.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/SkGpuDevice.h"
#include "wtf/MathExtras.h"
#include "wtf/text/Base64.h"
#include "wtf/text/WTFString.h"

using namespace std;

namespace WebCore {

static PassRefPtr<SkCanvas> createAcceleratedCanvas(const IntSize& size, Canvas2DLayerBridgePtr* outLayerBridge, OpacityMode opacityMode)
{
    RefPtr<GraphicsContext3D> context3D = SharedGraphicsContext3D::get();
    if (!context3D)
        return 0;
    Canvas2DLayerBridge::OpacityMode bridgeOpacityMode = opacityMode == Opaque ? Canvas2DLayerBridge::Opaque : Canvas2DLayerBridge::NonOpaque;
    *outLayerBridge = Canvas2DLayerBridge::create(context3D.release(), size, bridgeOpacityMode);
    // If canvas buffer allocation failed, debug build will have asserted
    // For release builds, we must verify whether the device has a render target
    return (*outLayerBridge) ? (*outLayerBridge)->getCanvas() : 0;
}

static PassRefPtr<SkCanvas> createNonPlatformCanvas(const IntSize& size)
{
    SkAutoTUnref<SkBaseDevice> device(new SkBitmapDevice(SkBitmap::kARGB_8888_Config, size.width(), size.height()));
    SkPixelRef* pixelRef = device->accessBitmap(false).pixelRef();
    return adoptRef(pixelRef ? new SkCanvas(device) : 0);
}

PassOwnPtr<ImageBuffer> ImageBuffer::createCompatibleBuffer(const IntSize& size, float resolutionScale, const GraphicsContext* context, bool hasAlpha)
{
    bool success = false;
    OwnPtr<ImageBuffer> buf = adoptPtr(new ImageBuffer(size, resolutionScale, context, hasAlpha, success));
    if (!success)
        return nullptr;
    return buf.release();
}

ImageBuffer::ImageBuffer(const IntSize& size, float resolutionScale, const GraphicsContext* compatibleContext, bool hasAlpha, bool& success)
    : m_size(size)
    , m_logicalSize(size)
    , m_resolutionScale(resolutionScale)
{
    if (!compatibleContext) {
        success = false;
        return;
    }

    SkAutoTUnref<SkBaseDevice> device(compatibleContext->createCompatibleDevice(size, hasAlpha));
    if (!device.get()) {
        success = false;
        return;
    }

    SkPixelRef* pixelRef = device->accessBitmap(false).pixelRef();
    if (!pixelRef) {
        success = false;
        return;
    }

    m_canvas = adoptRef(new SkCanvas(device));
    m_context = adoptPtr(new GraphicsContext(m_canvas.get()));
    m_context->setCertainlyOpaque(!hasAlpha);
    m_context->scale(FloatSize(m_resolutionScale, m_resolutionScale));

    success = true;
}

ImageBuffer::ImageBuffer(const IntSize& size, float resolutionScale, RenderingMode renderingMode, OpacityMode opacityMode, bool& success)
    : m_size(size)
    , m_logicalSize(size)
    , m_resolutionScale(resolutionScale)
{
    if (renderingMode == Accelerated) {
        m_canvas = createAcceleratedCanvas(size, &m_layerBridge, opacityMode);
        if (!m_canvas)
            renderingMode = UnacceleratedNonPlatformBuffer;
    }

    if (renderingMode == UnacceleratedNonPlatformBuffer)
        m_canvas = createNonPlatformCanvas(size);

    if (!m_canvas)
        m_canvas = adoptRef(skia::TryCreateBitmapCanvas(size.width(), size.height(), false));

    if (!m_canvas) {
        success = false;
        return;
    }

    m_context = adoptPtr(new GraphicsContext(m_canvas.get()));
    m_context->setCertainlyOpaque(opacityMode == Opaque);
    m_context->setAccelerated(renderingMode == Accelerated);
    m_context->scale(FloatSize(m_resolutionScale, m_resolutionScale));

    // Clear the background transparent or opaque, as required. It would be nice if this wasn't
    // required, but the canvas is currently filled with the magic transparency
    // color. Can we have another way to manage this?
    if (opacityMode == Opaque)
        m_canvas->drawARGB(255, 0, 0, 0, SkXfermode::kSrc_Mode);
    else
        m_canvas->drawARGB(0, 0, 0, 0, SkXfermode::kClear_Mode);

    success = true;
}

ImageBuffer::~ImageBuffer()
{
}

GraphicsContext* ImageBuffer::context() const
{
    if (m_layerBridge) {
        // We're using context acquisition as a signal that someone is about to render into our buffer and we need
        // to be ready. This isn't logically const-correct, hence the cast.
        const_cast<Canvas2DLayerBridge*>(m_layerBridge.get())->contextAcquired();
    }
    return m_context.get();
}


bool ImageBuffer::isValid() const
{
    if (m_layerBridge)
        return const_cast<Canvas2DLayerBridge*>(m_layerBridge.get())->isValid();
    return true;
}

static SkBitmap deepSkBitmapCopy(const SkBitmap& bitmap)
{
    SkBitmap tmp;
    if (!bitmap.deepCopyTo(&tmp, bitmap.config()))
        bitmap.copyTo(&tmp, bitmap.config());

    return tmp;
}

PassRefPtr<Image> ImageBuffer::copyImage(BackingStoreCopy copyBehavior, ScaleBehavior) const
{
    if (!isValid())
        return BitmapImage::create(NativeImageSkia::create());

    const SkBitmap& bitmap = *context()->bitmap();
    // FIXME: Start honoring ScaleBehavior to scale 2x buffers down to 1x.
    return BitmapImage::create(NativeImageSkia::create(copyBehavior == CopyBackingStore ? deepSkBitmapCopy(bitmap) : bitmap, m_resolutionScale));
}

BackingStoreCopy ImageBuffer::fastCopyImageMode()
{
    return DontCopyBackingStore;
}

WebKit::WebLayer* ImageBuffer::platformLayer() const
{
    return m_layerBridge ? m_layerBridge->layer() : 0;
}

bool ImageBuffer::copyToPlatformTexture(GraphicsContext3D& context, Platform3DObject texture, GC3Denum internalFormat, GC3Denum destType, GC3Dint level, bool premultiplyAlpha, bool flipY)
{
    if (!m_layerBridge || !platformLayer() || !isValid())
        return false;

    Platform3DObject sourceTexture = m_layerBridge->backBufferTexture();

    if (!context.makeContextCurrent())
        return false;

    Extensions3D* extensions = context.getExtensions();
    if (!extensions->supports("GL_CHROMIUM_copy_texture") || !extensions->supports("GL_CHROMIUM_flipy")
        || !extensions->canUseCopyTextureCHROMIUM(internalFormat, destType, level))
        return false;

    // The canvas is stored in a premultiplied format, so unpremultiply if necessary.
    context.pixelStorei(Extensions3D::UNPACK_UNPREMULTIPLY_ALPHA_CHROMIUM, !premultiplyAlpha);

    // The canvas is stored in an inverted position, so the flip semantics are reversed.
    context.pixelStorei(Extensions3D::UNPACK_FLIP_Y_CHROMIUM, !flipY);

    extensions->copyTextureCHROMIUM(GraphicsContext3D::TEXTURE_2D, sourceTexture, texture, level, internalFormat, destType);

    context.pixelStorei(Extensions3D::UNPACK_FLIP_Y_CHROMIUM, false);
    context.pixelStorei(Extensions3D::UNPACK_UNPREMULTIPLY_ALPHA_CHROMIUM, false);
    context.flush();
    return true;
}

static bool drawNeedsCopy(GraphicsContext* src, GraphicsContext* dst)
{
    return (src == dst);
}

void ImageBuffer::draw(GraphicsContext* context, const FloatRect& destRect, const FloatRect& srcRect,
    CompositeOperator op, BlendMode blendMode, bool useLowQualityScale)
{
    if (!isValid())
        return;

    const SkBitmap& bitmap = *m_context->bitmap();
    RefPtr<Image> image = BitmapImage::create(NativeImageSkia::create(drawNeedsCopy(m_context.get(), context) ? deepSkBitmapCopy(bitmap) : bitmap));
    context->drawImage(image.get(), destRect, srcRect, op, blendMode, DoNotRespectImageOrientation, useLowQualityScale);
}

void ImageBuffer::drawPattern(GraphicsContext* context, const FloatRect& srcRect, const FloatSize& scale,
    const FloatPoint& phase, CompositeOperator op, const FloatRect& destRect, BlendMode blendMode)
{
    if (!isValid())
        return;

    const SkBitmap& bitmap = *m_context->bitmap();
    RefPtr<Image> image = BitmapImage::create(NativeImageSkia::create(drawNeedsCopy(m_context.get(), context) ? deepSkBitmapCopy(bitmap) : bitmap));
    image->drawPattern(context, srcRect, scale, phase, op, destRect, blendMode);
}

static const Vector<uint8_t>& getLinearRgbLUT()
{
    DEFINE_STATIC_LOCAL(Vector<uint8_t>, linearRgbLUT, ());
    if (linearRgbLUT.isEmpty()) {
        linearRgbLUT.reserveCapacity(256);
        for (unsigned i = 0; i < 256; i++) {
            float color = i  / 255.0f;
            color = (color <= 0.04045f ? color / 12.92f : pow((color + 0.055f) / 1.055f, 2.4f));
            color = std::max(0.0f, color);
            color = std::min(1.0f, color);
            linearRgbLUT.append(static_cast<uint8_t>(round(color * 255)));
        }
    }
    return linearRgbLUT;
}

static const Vector<uint8_t>& getDeviceRgbLUT()
{
    DEFINE_STATIC_LOCAL(Vector<uint8_t>, deviceRgbLUT, ());
    if (deviceRgbLUT.isEmpty()) {
        deviceRgbLUT.reserveCapacity(256);
        for (unsigned i = 0; i < 256; i++) {
            float color = i / 255.0f;
            color = (powf(color, 1.0f / 2.4f) * 1.055f) - 0.055f;
            color = std::max(0.0f, color);
            color = std::min(1.0f, color);
            deviceRgbLUT.append(static_cast<uint8_t>(round(color * 255)));
        }
    }
    return deviceRgbLUT;
}

void ImageBuffer::transformColorSpace(ColorSpace srcColorSpace, ColorSpace dstColorSpace)
{
    if (srcColorSpace == dstColorSpace)
        return;

    // only sRGB <-> linearRGB are supported at the moment
    if ((srcColorSpace != ColorSpaceLinearRGB && srcColorSpace != ColorSpaceDeviceRGB)
        || (dstColorSpace != ColorSpaceLinearRGB && dstColorSpace != ColorSpaceDeviceRGB))
        return;

    // FIXME: Disable color space conversions on accelerated canvases (for now).
    if (context()->isAccelerated() || !isValid())
        return;

    const SkBitmap& bitmap = *context()->bitmap();
    if (bitmap.isNull())
        return;

    const Vector<uint8_t>& lookUpTable = dstColorSpace == ColorSpaceLinearRGB ?
        getLinearRgbLUT() : getDeviceRgbLUT();

    ASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);
    SkAutoLockPixels bitmapLock(bitmap);
    for (int y = 0; y < m_size.height(); ++y) {
        uint32_t* srcRow = bitmap.getAddr32(0, y);
        for (int x = 0; x < m_size.width(); ++x) {
            SkColor color = SkPMColorToColor(srcRow[x]);
            srcRow[x] = SkPreMultiplyARGB(
                SkColorGetA(color),
                lookUpTable[SkColorGetR(color)],
                lookUpTable[SkColorGetG(color)],
                lookUpTable[SkColorGetB(color)]);
        }
    }
}

PassRefPtr<SkColorFilter> ImageBuffer::createColorSpaceFilter(ColorSpace srcColorSpace,
    ColorSpace dstColorSpace)
{
    if ((srcColorSpace == dstColorSpace)
        || (srcColorSpace != ColorSpaceLinearRGB && srcColorSpace != ColorSpaceDeviceRGB)
        || (dstColorSpace != ColorSpaceLinearRGB && dstColorSpace != ColorSpaceDeviceRGB))
        return 0;

    const uint8_t* lut = 0;
    if (dstColorSpace == ColorSpaceLinearRGB)
        lut = &getLinearRgbLUT()[0];
    else if (dstColorSpace == ColorSpaceDeviceRGB)
        lut = &getDeviceRgbLUT()[0];
    else
        return 0;

    return adoptRef(SkTableColorFilter::CreateARGB(0, lut, lut, lut));
}

template <Multiply multiplied>
PassRefPtr<Uint8ClampedArray> getImageData(const IntRect& rect, GraphicsContext* context, const IntSize& size)
{
    float area = 4.0f * rect.width() * rect.height();
    if (area > static_cast<float>(std::numeric_limits<int>::max()))
        return 0;

    RefPtr<Uint8ClampedArray> result = Uint8ClampedArray::createUninitialized(rect.width() * rect.height() * 4);

    unsigned char* data = result->data();

    if (rect.x() < 0
        || rect.y() < 0
        || rect.maxX() > size.width()
        || rect.maxY() > size.height())
        result->zeroFill();

    unsigned destBytesPerRow = 4 * rect.width();
    SkBitmap destBitmap;
    destBitmap.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height(), destBytesPerRow);
    destBitmap.setPixels(data);

    SkCanvas::Config8888 config8888;
    if (multiplied == Premultiplied)
        config8888 = SkCanvas::kRGBA_Premul_Config8888;
    else
        config8888 = SkCanvas::kRGBA_Unpremul_Config8888;

    context->readPixels(&destBitmap, rect.x(), rect.y(), config8888);
    return result.release();
}

PassRefPtr<Uint8ClampedArray> ImageBuffer::getUnmultipliedImageData(const IntRect& rect, CoordinateSystem) const
{
    if (!isValid())
        return Uint8ClampedArray::create(rect.width() * rect.height() * 4);
    return getImageData<Unmultiplied>(rect, context(), m_size);
}

PassRefPtr<Uint8ClampedArray> ImageBuffer::getPremultipliedImageData(const IntRect& rect, CoordinateSystem) const
{
    if (!isValid())
        return Uint8ClampedArray::create(rect.width() * rect.height() * 4);
    return getImageData<Premultiplied>(rect, context(), m_size);
}

void ImageBuffer::putByteArray(Multiply multiplied, Uint8ClampedArray* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, CoordinateSystem)
{
    if (!isValid())
        return;

    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originX = sourceRect.x();
    int destX = destPoint.x() + sourceRect.x();
    ASSERT(destX >= 0);
    ASSERT(destX < m_size.width());
    ASSERT(originX >= 0);
    ASSERT(originX < sourceRect.maxX());

    int endX = destPoint.x() + sourceRect.maxX();
    ASSERT(endX <= m_size.width());

    int numColumns = endX - destX;

    int originY = sourceRect.y();
    int destY = destPoint.y() + sourceRect.y();
    ASSERT(destY >= 0);
    ASSERT(destY < m_size.height());
    ASSERT(originY >= 0);
    ASSERT(originY < sourceRect.maxY());

    int endY = destPoint.y() + sourceRect.maxY();
    ASSERT(endY <= m_size.height());
    int numRows = endY - destY;

    unsigned srcBytesPerRow = 4 * sourceSize.width();
    SkBitmap srcBitmap;
    srcBitmap.setConfig(SkBitmap::kARGB_8888_Config, numColumns, numRows, srcBytesPerRow);
    srcBitmap.setPixels(source->data() + originY * srcBytesPerRow + originX * 4);

    SkCanvas::Config8888 config8888;
    if (multiplied == Premultiplied)
        config8888 = SkCanvas::kRGBA_Premul_Config8888;
    else
        config8888 = SkCanvas::kRGBA_Unpremul_Config8888;

    context()->writePixels(srcBitmap, destX, destY, config8888);
}

template <typename T>
static bool encodeImage(T& source, const String& mimeType, const double* quality, Vector<char>* output)
{
    Vector<unsigned char>* encodedImage = reinterpret_cast<Vector<unsigned char>*>(output);

    if (mimeType == "image/jpeg") {
        int compressionQuality = JPEGImageEncoder::DefaultCompressionQuality;
        if (quality && *quality >= 0.0 && *quality <= 1.0)
            compressionQuality = static_cast<int>(*quality * 100 + 0.5);
        if (!JPEGImageEncoder::encode(source, compressionQuality, encodedImage))
            return false;
    } else if (mimeType == "image/webp") {
        int compressionQuality = WEBPImageEncoder::DefaultCompressionQuality;
        if (quality && *quality >= 0.0 && *quality <= 1.0)
            compressionQuality = static_cast<int>(*quality * 100 + 0.5);
        if (!WEBPImageEncoder::encode(source, compressionQuality, encodedImage))
            return false;
    } else {
        if (!PNGImageEncoder::encode(source, encodedImage))
            return false;
        ASSERT(mimeType == "image/png");
    }

    return true;
}

String ImageBuffer::toDataURL(const String& mimeType, const double* quality, CoordinateSystem) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    Vector<char> encodedImage;
    if (!isValid() || !encodeImage(*context()->bitmap(), mimeType, quality, &encodedImage))
        return "data:,";
    Vector<char> base64Data;
    base64Encode(encodedImage, base64Data);

    return "data:" + mimeType + ";base64," + base64Data;
}

String ImageDataToDataURL(const ImageData& imageData, const String& mimeType, const double* quality)
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    Vector<char> encodedImage;
    if (!encodeImage(imageData, mimeType, quality, &encodedImage))
        return "data:,";

    Vector<char> base64Data;
    base64Encode(encodedImage, base64Data);

    return "data:" + mimeType + ";base64," + base64Data;
}

} // namespace WebCore
