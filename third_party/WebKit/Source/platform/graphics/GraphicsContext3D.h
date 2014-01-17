/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef GraphicsContext3D_h
#define GraphicsContext3D_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsTypes3D.h"
#include "platform/graphics/Image.h"
#include "platform/weborigin/KURL.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/ListHashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

// FIXME: Find a better way to avoid the name confliction for NO_ERROR.
#if OS(WIN)
#undef NO_ERROR
#endif

class GrContext;

namespace blink {
class WebGraphicsContext3D;
class WebGraphicsContext3DProvider;
}

namespace WebCore {
class Image;
class ImageBuffer;
class IntRect;
class IntSize;

class PLATFORM_EXPORT GraphicsContext3D : public RefCounted<GraphicsContext3D> {
public:
    // This is the preferred method for creating an instance of this class. When created this way the webContext
    // is not owned by the GraphicsContext3D
    static PassRefPtr<GraphicsContext3D> createContextSupport(blink::WebGraphicsContext3D* webContext);

    // Callers must make the context current before using it AND check that the context was created successfully
    // via ContextLost before using the context in any way. Once made current on a thread, the context cannot
    // be used on any other thread.
    // This creation method is obsolete and should not be used by new code. They will be removed soon.
    static PassRefPtr<GraphicsContext3D> createGraphicsContextFromProvider(PassOwnPtr<blink::WebGraphicsContext3DProvider>);

    ~GraphicsContext3D();

    GrContext* grContext();
    blink::WebGraphicsContext3D* webContext() const { return m_impl; }

    //----------------------------------------------------------------------
    // Helpers for texture uploading and pixel readback.
    //

    // Computes the components per pixel and bytes per component
    // for the given format and type combination. Returns false if
    // either was an invalid enum.
    static bool computeFormatAndTypeParameters(GLenum format, GLenum type, unsigned* componentsPerPixel, unsigned* bytesPerComponent);

    // Computes the image size in bytes. If paddingInBytes is not null, padding
    // is also calculated in return. Returns NO_ERROR if succeed, otherwise
    // return the suggested GL error indicating the cause of the failure:
    //   INVALID_VALUE if width/height is negative or overflow happens.
    //   INVALID_ENUM if format/type is illegal.
    static GLenum computeImageSizeInBytes(GLenum format, GLenum type, GLsizei width, GLsizei height, GLint alignment, unsigned* imageSizeInBytes, unsigned* paddingInBytes);

    // Attempt to enumerate all possible native image formats to
    // reduce the amount of temporary allocations during texture
    // uploading. This enum must be public because it is accessed
    // by non-member functions.
    enum DataFormat {
        DataFormatRGBA8 = 0,
        DataFormatRGBA16F,
        DataFormatRGBA32F,
        DataFormatRGB8,
        DataFormatRGB16F,
        DataFormatRGB32F,
        DataFormatBGR8,
        DataFormatBGRA8,
        DataFormatARGB8,
        DataFormatABGR8,
        DataFormatRGBA5551,
        DataFormatRGBA4444,
        DataFormatRGB565,
        DataFormatR8,
        DataFormatR16F,
        DataFormatR32F,
        DataFormatRA8,
        DataFormatRA16F,
        DataFormatRA32F,
        DataFormatAR8,
        DataFormatA8,
        DataFormatA16F,
        DataFormatA32F,
        DataFormatNumFormats
    };

    // Check if the format is one of the formats from the ImageData or DOM elements.
    // The formats from ImageData is always RGBA8.
    // The formats from DOM elements vary with Graphics ports. It can only be RGBA8 or BGRA8.
    static ALWAYS_INLINE bool srcFormatComeFromDOMElementOrImageData(DataFormat SrcFormat)
    {
    return SrcFormat == DataFormatBGRA8 || SrcFormat == DataFormatRGBA8;
    }

    static unsigned getClearBitsByFormat(GLenum);

    enum ChannelBits {
        ChannelRed = 1,
        ChannelGreen = 2,
        ChannelBlue = 4,
        ChannelAlpha = 8,
        ChannelDepth = 16,
        ChannelStencil = 32,
        ChannelRGB = ChannelRed | ChannelGreen | ChannelBlue,
        ChannelRGBA = ChannelRGB | ChannelAlpha,
    };

    static unsigned getChannelBitsByFormat(GLenum);

    // Possible alpha operations that may need to occur during
    // pixel packing. FIXME: kAlphaDoUnmultiply is lossy and must
    // be removed.
    enum AlphaOp {
        AlphaDoNothing = 0,
        AlphaDoPremultiply = 1,
        AlphaDoUnmultiply = 2
    };

    enum ImageHtmlDomSource {
        HtmlDomImage = 0,
        HtmlDomCanvas = 1,
        HtmlDomVideo = 2,
        HtmlDomNone = 3
    };

    class PLATFORM_EXPORT ImageExtractor {
    public:
        ImageExtractor(Image*, ImageHtmlDomSource, bool premultiplyAlpha, bool ignoreGammaAndColorProfile);

        ~ImageExtractor();

        bool extractSucceeded() { return m_extractSucceeded; }
        const void* imagePixelData() { return m_imagePixelData; }
        unsigned imageWidth() { return m_imageWidth; }
        unsigned imageHeight() { return m_imageHeight; }
        DataFormat imageSourceFormat() { return m_imageSourceFormat; }
        AlphaOp imageAlphaOp() { return m_alphaOp; }
        unsigned imageSourceUnpackAlignment() { return m_imageSourceUnpackAlignment; }
        ImageHtmlDomSource imageHtmlDomSource() { return m_imageHtmlDomSource; }
    private:
        // Extract the image and keeps track of its status, such as width, height, Source Alignment, format and AlphaOp etc.
        // This needs to lock the resources or relevant data if needed and return true upon success
        bool extractImage(bool premultiplyAlpha, bool ignoreGammaAndColorProfile);

        RefPtr<NativeImageSkia> m_nativeImage;
        RefPtr<NativeImageSkia> m_skiaImage;
        Image* m_image;
        ImageHtmlDomSource m_imageHtmlDomSource;
        bool m_extractSucceeded;
        const void* m_imagePixelData;
        unsigned m_imageWidth;
        unsigned m_imageHeight;
        DataFormat m_imageSourceFormat;
        AlphaOp m_alphaOp;
        unsigned m_imageSourceUnpackAlignment;
    };

    // The Following functions are implemented in GraphicsContext3DImagePacking.cpp

    // Packs the contents of the given Image which is passed in |pixels| into the passed Vector
    // according to the given format and type, and obeying the flipY and AlphaOp flags.
    // Returns true upon success.
    static bool packImageData(Image*, const void* pixels, GLenum format, GLenum type, bool flipY, AlphaOp, DataFormat sourceFormat, unsigned width, unsigned height, unsigned sourceUnpackAlignment, Vector<uint8_t>& data);

    // Extracts the contents of the given ImageData into the passed Vector,
    // packing the pixel data according to the given format and type,
    // and obeying the flipY and premultiplyAlpha flags. Returns true
    // upon success.
    static bool extractImageData(const uint8_t*, const IntSize&, GLenum format, GLenum type, bool flipY, bool premultiplyAlpha, Vector<uint8_t>& data);

    // Helper function which extracts the user-supplied texture
    // data, applying the flipY and premultiplyAlpha parameters.
    // If the data is not tightly packed according to the passed
    // unpackAlignment, the output data will be tightly packed.
    // Returns true if successful, false if any error occurred.
    static bool extractTextureData(unsigned width, unsigned height, GLenum format, GLenum type, unsigned unpackAlignment, bool flipY, bool premultiplyAlpha, const void* pixels, Vector<uint8_t>& data);

    // End GraphicsContext3DImagePacking.cpp functions

    // Extension support.
    bool supportsExtension(const String& name);
    bool ensureExtensionEnabled(const String& name);
    bool isExtensionEnabled(const String& name);

    static bool canUseCopyTextureCHROMIUM(GLenum destFormat, GLenum destType, GLint level);

private:
    GraphicsContext3D(PassOwnPtr<blink::WebGraphicsContext3DProvider>);
    GraphicsContext3D(blink::WebGraphicsContext3D* webContext);

    // Helper for packImageData/extractImageData/extractTextureData which implement packing of pixel
    // data into the specified OpenGL destination format and type.
    // A sourceUnpackAlignment of zero indicates that the source
    // data is tightly packed. Non-zero values may take a slow path.
    // Destination data will have no gaps between rows.
    // Implemented in GraphicsContext3DImagePacking.cpp
    static bool packPixels(const uint8_t* sourceData, DataFormat sourceDataFormat, unsigned width, unsigned height, unsigned sourceUnpackAlignment, unsigned destinationFormat, unsigned destinationType, AlphaOp, void* destinationData, bool flipY);

    void initializeExtensions();

    OwnPtr<blink::WebGraphicsContext3DProvider> m_provider;
    blink::WebGraphicsContext3D* m_impl;
    bool m_initializedAvailableExtensions;
    HashSet<String> m_enabledExtensions;
    HashSet<String> m_requestableExtensions;
    GrContext* m_grContext;
};

} // namespace WebCore

#endif // GraphicsContext3D_h
