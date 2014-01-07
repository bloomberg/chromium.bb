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
#include "platform/graphics/Extensions3D.h"
#include "platform/graphics/GraphicsTypes3D.h"
#include "platform/graphics/Image.h"
#include "platform/weborigin/KURL.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
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
class DrawingBuffer;
class Extensions3D;
class GraphicsContext3DContextLostCallbackAdapter;
class GraphicsContext3DErrorMessageCallbackAdapter;
class Image;
class ImageBuffer;
class IntRect;
class IntSize;

struct ActiveInfo {
    String name;
    GLenum type;
    GLint size;
};

class PLATFORM_EXPORT GraphicsContext3D : public RefCounted<GraphicsContext3D> {
public:
    // Context creation attributes.
    struct Attributes {
        Attributes()
            : alpha(true)
            , depth(true)
            , stencil(false)
            , antialias(true)
            , premultipliedAlpha(true)
            , preserveDrawingBuffer(false)
            , noExtensions(false)
            , shareResources(true)
            , preferDiscreteGPU(false)
            , failIfMajorPerformanceCaveat(false)
        {
        }

        bool alpha;
        bool depth;
        bool stencil;
        bool antialias;
        bool premultipliedAlpha;
        bool preserveDrawingBuffer;
        bool noExtensions;
        bool shareResources;
        bool preferDiscreteGPU;
        bool failIfMajorPerformanceCaveat;
        KURL topDocumentURL;
    };

    class ContextLostCallback {
    public:
        virtual void onContextLost() = 0;
        virtual ~ContextLostCallback() {}
    };

    class ErrorMessageCallback {
    public:
        virtual void onErrorMessage(const String& message, GLint id) = 0;
        virtual ~ErrorMessageCallback() { }
    };

    void setContextLostCallback(PassOwnPtr<ContextLostCallback>);
    void setErrorMessageCallback(PassOwnPtr<ErrorMessageCallback>);

    // This is the preferred method for creating an instance of this class. When created this way the webContext
    // is not owned by the GraphicsContext3D
    static PassRefPtr<GraphicsContext3D> createContextSupport(blink::WebGraphicsContext3D* webContext);

    // The following three creation methods are obsolete and should not be used by new code. They will be removed soon.

    // Callers must make the context current before using it AND check that the context was created successfully
    // via ContextLost before using the context in any way. Once made current on a thread, the context cannot
    // be used on any other thread.
    static PassRefPtr<GraphicsContext3D> create(Attributes);
    static PassRefPtr<GraphicsContext3D> createGraphicsContextFromWebContext(PassOwnPtr<blink::WebGraphicsContext3D>, bool preserveDrawingBuffer = false);
    static PassRefPtr<GraphicsContext3D> createGraphicsContextFromProvider(PassOwnPtr<blink::WebGraphicsContext3DProvider>, bool preserveDrawingBuffer = false);


    ~GraphicsContext3D();

    GrContext* grContext();
    blink::WebGraphicsContext3D* webContext() const { return m_impl; }

    bool makeContextCurrent();

    uint32_t lastFlushID();

    // Helper to texImage2D with pixel==0 case: pixels are initialized to 0.
    // Return true if no GL error is synthesized.
    // By default, alignment is 4, the OpenGL default setting.
    bool texImage2DResourceSafe(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLint alignment = 4);

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

    //----------------------------------------------------------------------
    // Entry points for WebGL.
    // These are obsolete and should not be called by new code.
    // Prefer calling WebGraphicsContext3D methods directly instead.
    //

    void activeTexture(GLenum texture);
    void attachShader(Platform3DObject program, Platform3DObject shader);
    void bindBuffer(GLenum target, Platform3DObject);
    void bindFramebuffer(GLenum target, Platform3DObject);
    void bindRenderbuffer(GLenum target, Platform3DObject);
    void bindTexture(GLenum target, Platform3DObject);

    void bufferData(GLenum target, GLsizeiptr, const void* data, GLenum usage);

    GLenum checkFramebufferStatus(GLenum target);
    void clear(GLbitfield mask);
    void clearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
    void clearDepth(GLclampf depth);
    void clearStencil(GLint s);
    void colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
    void compileShader(Platform3DObject);

    void depthMask(GLboolean flag);
    void disable(GLenum cap);
    void disableVertexAttribArray(GLuint index);
    void drawElements(GLenum mode, GLsizei count, GLenum type, GLintptr offset);

    void enable(GLenum cap);
    void enableVertexAttribArray(GLuint index);
    void finish();
    void flush();
    void framebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, Platform3DObject);
    void framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, Platform3DObject, GLint level);

    bool getActiveAttrib(Platform3DObject program, GLuint index, ActiveInfo&);
    GLint getAttribLocation(Platform3DObject, const String& name);
    Attributes getContextAttributes();
    GLenum getError();
    void getIntegerv(GLenum pname, GLint* value);
    void getProgramiv(Platform3DObject program, GLenum pname, GLint* value);
    void getShaderiv(Platform3DObject, GLenum pname, GLint* value);
    String getString(GLenum name);
    GLint getUniformLocation(Platform3DObject, const String& name);

    void linkProgram(Platform3DObject);
    void pixelStorei(GLenum pname, GLint param);

    void readPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* data);

    void renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
    void shaderSource(Platform3DObject, const String& string);
    void stencilMaskSeparate(GLenum face, GLuint mask);

    void texImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
    void texParameteri(GLenum target, GLenum pname, GLint param);

    void uniform1f(GLint location, GLfloat x);
    void uniform1fv(GLint location, GLsizei, GLfloat* v);
    void uniform1i(GLint location, GLint x);
    void uniform2f(GLint location, GLfloat x, GLfloat y);
    void uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z);
    void uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
    void uniformMatrix4fv(GLint location, GLsizei, GLboolean transpose, GLfloat* value);

    void useProgram(Platform3DObject);

    void vertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset);

    void viewport(GLint x, GLint y, GLsizei width, GLsizei height);

    void markContextChanged();
    void markLayerComposited();
    bool layerComposited() const;

    void paintRenderingResultsToCanvas(ImageBuffer*, DrawingBuffer*);
    PassRefPtr<Uint8ClampedArray> paintRenderingResultsToImageData(DrawingBuffer*, int&, int&);

    // Support for buffer creation and deletion
    Platform3DObject createBuffer();
    Platform3DObject createFramebuffer();
    Platform3DObject createProgram();
    Platform3DObject createRenderbuffer();
    Platform3DObject createShader(GLenum);
    Platform3DObject createTexture();

    void deleteBuffer(Platform3DObject);
    void deleteFramebuffer(Platform3DObject);
    void deleteProgram(Platform3DObject);
    void deleteRenderbuffer(Platform3DObject);
    void deleteShader(Platform3DObject);
    void deleteTexture(Platform3DObject);

    // Synthesizes an OpenGL error which will be returned from a
    // later call to getError. This is used to emulate OpenGL ES
    // 2.0 behavior on the desktop and to enforce additional error
    // checking mandated by WebGL.
    //
    // Per the behavior of glGetError, this stores at most one
    // instance of any given error, and returns them from calls to
    // getError in the order they were added.
    void synthesizeGLError(GLenum error);

    // Support for extensions. Returns a non-null object, though not
    // all methods it contains may necessarily be supported on the
    // current hardware. Must call Extensions3D::supports() to
    // determine this.
    Extensions3D* extensions();

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

    // This is the order of bytes to use when doing a readback.
    enum ReadbackOrder {
        ReadbackRGBA,
        ReadbackSkia
    };

    // Helper function which does a readback from the currently-bound
    // framebuffer into a buffer of a certain size with 4-byte pixels.
    void readBackFramebuffer(unsigned char* pixels, int width, int height, ReadbackOrder, AlphaOp);

    void setPackAlignment(GLint param);

    // Extensions3D support.
    bool supportsExtension(const String& name);
    bool ensureExtensionEnabled(const String& name);
    bool isExtensionEnabled(const String& name);

private:
    friend class Extensions3D;

    GraphicsContext3D(PassOwnPtr<blink::WebGraphicsContext3D>, bool preserveDrawingBuffer);
    GraphicsContext3D(PassOwnPtr<blink::WebGraphicsContext3DProvider>, bool preserveDrawingBuffer);
    GraphicsContext3D(blink::WebGraphicsContext3D* webContext);

    // Helper for packImageData/extractImageData/extractTextureData which implement packing of pixel
    // data into the specified OpenGL destination format and type.
    // A sourceUnpackAlignment of zero indicates that the source
    // data is tightly packed. Non-zero values may take a slow path.
    // Destination data will have no gaps between rows.
    // Implemented in GraphicsContext3DImagePacking.cpp
    static bool packPixels(const uint8_t* sourceData, DataFormat sourceDataFormat, unsigned width, unsigned height, unsigned sourceUnpackAlignment, unsigned destinationFormat, unsigned destinationType, AlphaOp, void* destinationData, bool flipY);

    void paintFramebufferToCanvas(int framebuffer, int width, int height, bool premultiplyAlpha, ImageBuffer*);
    // Helper function to flip a bitmap vertically.
    void flipVertically(uint8_t* data, int width, int height);

    void initializeExtensions();

    bool preserveDrawingBuffer() const { return m_preserveDrawingBuffer; }

    OwnPtr<blink::WebGraphicsContext3DProvider> m_provider;
    blink::WebGraphicsContext3D* m_impl;
    OwnPtr<GraphicsContext3DContextLostCallbackAdapter> m_contextLostCallbackAdapter;
    OwnPtr<GraphicsContext3DErrorMessageCallbackAdapter> m_errorMessageCallbackAdapter;
    OwnPtr<blink::WebGraphicsContext3D> m_ownedWebContext;
    OwnPtr<Extensions3D> m_extensions;
    bool m_initializedAvailableExtensions;
    HashSet<String> m_enabledExtensions;
    HashSet<String> m_requestableExtensions;
    bool m_layerComposited;
    bool m_preserveDrawingBuffer;
    int m_packAlignment;

    // If the width and height of the Canvas's backing store don't
    // match those that we were given in the most recent call to
    // reshape(), then we need an intermediate bitmap to read back the
    // frame buffer into. This seems to happen when CSS styles are
    // used to resize the Canvas.
    SkBitmap m_resizingBitmap;

    GrContext* m_grContext;

    // Used to flip a bitmap vertically.
    Vector<uint8_t> m_scanline;
};

} // namespace WebCore

#endif // GraphicsContext3D_h
