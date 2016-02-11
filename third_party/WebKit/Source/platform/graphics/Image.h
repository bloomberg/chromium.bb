/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef Image_h
#define Image_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/ImageAnimationPolicy.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/ImageOrientation.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "wtf/Assertions.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

class SkBitmap;
class SkImage;

namespace blink {

class FloatPoint;
class FloatRect;
class FloatSize;
class GraphicsContext;
class Length;
class SharedBuffer;
class Image;

class PLATFORM_EXPORT Image : public RefCounted<Image> {
    friend class GeneratedImage;
    friend class CrossfadeGeneratedImage;
    friend class GradientGeneratedImage;
    friend class GraphicsContext;
    WTF_MAKE_NONCOPYABLE(Image);

public:
    virtual ~Image();

    static PassRefPtr<Image> loadPlatformResource(const char* name);
    static bool supportsType(const String&);

    virtual bool isSVGImage() const { return false; }
    virtual bool isBitmapImage() const { return false; }

    // To increase accuracy of currentFrameKnownToBeOpaque() it may,
    // for applicable image types, be told to pre-cache metadata for
    // the current frame. Since this may initiate a deferred image
    // decoding, PreCacheMetadata requires a InspectorPaintImageEvent
    // during call.
    enum MetadataMode { UseCurrentMetadata, PreCacheMetadata };
    virtual bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata) = 0;

    virtual bool currentFrameIsComplete() { return false; }
    virtual bool currentFrameIsLazyDecoded() { return false; }
    virtual bool isTextureBacked();

    // Derived classes should override this if they can assure that the current
    // image frame contains only resources from its own security origin.
    virtual bool currentFrameHasSingleSecurityOrigin() const { return false; }

    static Image* nullImage();
    bool isNull() const { return size().isEmpty(); }

    virtual bool usesContainerSize() const { return false; }
    virtual bool hasRelativeSize() const { return false; }

    // Computes (extracts) the intrinsic dimensions and ratio from the Image. The intrinsic ratio
    // will be the 'viewport' of the image. (Same as the dimensions for a raster image. For SVG
    // images it can be the dimensions defined by the 'viewBox'.)
    virtual void computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio);

    virtual IntSize size() const = 0;
    IntRect rect() const { return IntRect(IntPoint(), size()); }
    int width() const { return size().width(); }
    int height() const { return size().height(); }
    virtual bool getHotSpot(IntPoint&) const { return false; }

    bool setData(PassRefPtr<SharedBuffer> data, bool allDataReceived);
    virtual bool dataChanged(bool /*allDataReceived*/) { return false; }

    virtual String filenameExtension() const { return String(); } // null string if unknown

    virtual void destroyDecodedData(bool destroyAll) = 0;

    SharedBuffer* data() { return m_encodedImageData.get(); }

    // Animation begins whenever someone draws the image, so startAnimation() is not normally called.
    // It will automatically pause once all observers no longer want to render the image anywhere.
    enum CatchUpAnimation { DoNotCatchUp, CatchUp };
    virtual void startAnimation(CatchUpAnimation = CatchUp) { }
    virtual void stopAnimation() {}
    virtual void resetAnimation() {}

    // True if this image can potentially animate.
    virtual bool maybeAnimated() { return false; }

    // Set animationPolicy
    virtual void setAnimationPolicy(ImageAnimationPolicy) { }
    virtual ImageAnimationPolicy animationPolicy() { return ImageAnimationPolicyAllowed; }
    virtual void advanceTime(double deltaTimeInSeconds) { }

    // Advances an animated image. For BitmapImage (e.g., animated gifs) this
    // will advance to the next frame. For SVGImage, this will trigger an
    // animation update for CSS and advance the SMIL timeline by one frame.
    virtual void advanceAnimationForTesting() { }

    // Typically the ImageResource that owns us.
    ImageObserver* imageObserver() const { return m_imageObserver; }
    void setImageObserver(ImageObserver* observer) { m_imageObserver = observer; }

    enum TileRule { StretchTile, RoundTile, SpaceTile, RepeatTile };

    virtual PassRefPtr<SkImage> imageForCurrentFrame() = 0;
    virtual PassRefPtr<Image> imageForDefaultFrame();

    virtual void drawPattern(GraphicsContext&, const FloatRect&,
        const FloatSize&, const FloatPoint& phase, SkXfermode::Mode,
        const FloatRect&, const FloatSize& repeatSpacing = FloatSize());

    enum ImageClampingMode {
        ClampImageToSourceRect,
        DoNotClampImageToSourceRect
    };

    virtual void draw(SkCanvas*, const SkPaint&, const FloatRect& dstRect, const FloatRect& srcRect, RespectImageOrientationEnum, ImageClampingMode) = 0;

protected:
    Image(ImageObserver* = 0);

    void drawTiled(GraphicsContext&, const FloatRect& dstRect, const FloatPoint& srcPoint, const FloatSize& tileSize,
        SkXfermode::Mode, const FloatSize& repeatSpacing);
    void drawTiled(GraphicsContext&, const FloatRect& dstRect, const FloatRect& srcRect, const FloatSize& tileScaleFactor, TileRule hRule, TileRule vRule, SkXfermode::Mode);

private:
    RefPtr<SharedBuffer> m_encodedImageData;
    // TODO(Oilpan): consider having Image on the Oilpan heap and
    // turn this into a Member<>.
    //
    // The observer (an ImageResource) is an untraced member, with the ImageResource
    // being responsible of clearing itself out.
    RawPtrWillBeUntracedMember<ImageObserver> m_imageObserver;
};

#define DEFINE_IMAGE_TYPE_CASTS(typeName) \
    DEFINE_TYPE_CASTS(typeName, Image, image, image->is##typeName(), image.is##typeName())

} // namespace blink

#endif
