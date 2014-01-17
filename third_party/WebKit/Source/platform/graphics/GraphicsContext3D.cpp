/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Mozilla Corporation. All rights reserved.
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

#include "config.h"

#include "platform/graphics/GraphicsContext3D.h"
#include "platform/CheckedInt.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringHash.h"

#include "public/platform/WebGraphicsContext3D.h"
#include "public/platform/WebGraphicsContext3DProvider.h"

namespace WebCore {

GraphicsContext3D::GraphicsContext3D(PassOwnPtr<blink::WebGraphicsContext3DProvider> provider)
    : m_provider(provider)
    , m_impl(m_provider->context3d())
    , m_initializedAvailableExtensions(false)
    , m_grContext(m_provider->grContext())
{
}

GraphicsContext3D::GraphicsContext3D(blink::WebGraphicsContext3D* webContext)
    : m_impl(webContext)
    , m_initializedAvailableExtensions(false)
    , m_grContext(0)
{
}

GraphicsContext3D::~GraphicsContext3D()
{
}

PassRefPtr<GraphicsContext3D> GraphicsContext3D::createContextSupport(blink::WebGraphicsContext3D* webContext)
{
    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(webContext));
    return context.release();
}

// This creation method is obsolete and should not be used by new code. They will be removed soon.
PassRefPtr<GraphicsContext3D> GraphicsContext3D::createGraphicsContextFromProvider(PassOwnPtr<blink::WebGraphicsContext3DProvider> provider)
{
    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(provider));
    return context.release();
}

GrContext* GraphicsContext3D::grContext()
{
    return m_grContext;
}

bool GraphicsContext3D::computeFormatAndTypeParameters(GLenum format, GLenum type, unsigned* componentsPerPixel, unsigned* bytesPerComponent)
{
    switch (format) {
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_STENCIL_OES:
        *componentsPerPixel = 1;
        break;
    case GL_LUMINANCE_ALPHA:
        *componentsPerPixel = 2;
        break;
    case GL_RGB:
        *componentsPerPixel = 3;
        break;
    case GL_RGBA:
    case GL_BGRA_EXT: // GL_EXT_texture_format_BGRA8888
        *componentsPerPixel = 4;
        break;
    default:
        return false;
    }
    switch (type) {
    case GL_UNSIGNED_BYTE:
        *bytesPerComponent = sizeof(GLubyte);
        break;
    case GL_UNSIGNED_SHORT:
        *bytesPerComponent = sizeof(GLushort);
        break;
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
        *componentsPerPixel = 1;
        *bytesPerComponent = sizeof(GLushort);
        break;
    case GL_UNSIGNED_INT_24_8_OES:
    case GL_UNSIGNED_INT:
        *bytesPerComponent = sizeof(GLuint);
        break;
    case GL_FLOAT: // OES_texture_float
        *bytesPerComponent = sizeof(GLfloat);
        break;
    case GL_HALF_FLOAT_OES: // OES_texture_half_float
        *bytesPerComponent = sizeof(GLushort);
        break;
    default:
        return false;
    }
    return true;
}

GLenum GraphicsContext3D::computeImageSizeInBytes(GLenum format, GLenum type, GLsizei width, GLsizei height, GLint alignment, unsigned* imageSizeInBytes, unsigned* paddingInBytes)
{
    ASSERT(imageSizeInBytes);
    ASSERT(alignment == 1 || alignment == 2 || alignment == 4 || alignment == 8);
    if (width < 0 || height < 0)
        return GL_INVALID_VALUE;
    unsigned bytesPerComponent, componentsPerPixel;
    if (!computeFormatAndTypeParameters(format, type, &bytesPerComponent, &componentsPerPixel))
        return GL_INVALID_ENUM;
    if (!width || !height) {
        *imageSizeInBytes = 0;
        if (paddingInBytes)
            *paddingInBytes = 0;
        return GL_NO_ERROR;
    }
    CheckedInt<uint32_t> checkedValue(bytesPerComponent * componentsPerPixel);
    checkedValue *=  width;
    if (!checkedValue.isValid())
        return GL_INVALID_VALUE;
    unsigned validRowSize = checkedValue.value();
    unsigned padding = 0;
    unsigned residual = validRowSize % alignment;
    if (residual) {
        padding = alignment - residual;
        checkedValue += padding;
    }
    // Last row needs no padding.
    checkedValue *= (height - 1);
    checkedValue += validRowSize;
    if (!checkedValue.isValid())
        return GL_INVALID_VALUE;
    *imageSizeInBytes = checkedValue.value();
    if (paddingInBytes)
        *paddingInBytes = padding;
    return GL_NO_ERROR;
}

GraphicsContext3D::ImageExtractor::ImageExtractor(Image* image, ImageHtmlDomSource imageHtmlDomSource, bool premultiplyAlpha, bool ignoreGammaAndColorProfile)
{
    m_image = image;
    m_imageHtmlDomSource = imageHtmlDomSource;
    m_extractSucceeded = extractImage(premultiplyAlpha, ignoreGammaAndColorProfile);
}

GraphicsContext3D::ImageExtractor::~ImageExtractor()
{
    if (m_skiaImage)
        m_skiaImage->bitmap().unlockPixels();
}

bool GraphicsContext3D::ImageExtractor::extractImage(bool premultiplyAlpha, bool ignoreGammaAndColorProfile)
{
    if (!m_image)
        return false;
    m_skiaImage = m_image->nativeImageForCurrentFrame();
    m_alphaOp = AlphaDoNothing;
    bool hasAlpha = m_skiaImage ? !m_skiaImage->bitmap().isOpaque() : true;
    if ((!m_skiaImage || ignoreGammaAndColorProfile || (hasAlpha && !premultiplyAlpha)) && m_image->data()) {
        // Attempt to get raw unpremultiplied image data.
        OwnPtr<ImageDecoder> decoder(ImageDecoder::create(
            *(m_image->data()), ImageSource::AlphaNotPremultiplied,
            ignoreGammaAndColorProfile ? ImageSource::GammaAndColorProfileIgnored : ImageSource::GammaAndColorProfileApplied));
        if (!decoder)
            return false;
        decoder->setData(m_image->data(), true);
        if (!decoder->frameCount())
            return false;
        ImageFrame* frame = decoder->frameBufferAtIndex(0);
        if (!frame || frame->status() != ImageFrame::FrameComplete)
            return false;
        hasAlpha = frame->hasAlpha();
        m_nativeImage = frame->asNewNativeImage();
        if (!m_nativeImage.get() || !m_nativeImage->isDataComplete() || !m_nativeImage->bitmap().width() || !m_nativeImage->bitmap().height())
            return false;
        SkBitmap::Config skiaConfig = m_nativeImage->bitmap().config();
        if (skiaConfig != SkBitmap::kARGB_8888_Config)
            return false;
        m_skiaImage = m_nativeImage.get();
        if (hasAlpha && premultiplyAlpha)
            m_alphaOp = AlphaDoPremultiply;
    } else if (!premultiplyAlpha && hasAlpha) {
        // 1. For texImage2D with HTMLVideoElment input, assume no PremultiplyAlpha had been applied and the alpha value for each pixel is 0xFF
        // which is true at present and may be changed in the future and needs adjustment accordingly.
        // 2. For texImage2D with HTMLCanvasElement input in which Alpha is already Premultiplied in this port,
        // do AlphaDoUnmultiply if UNPACK_PREMULTIPLY_ALPHA_WEBGL is set to false.
        if (m_imageHtmlDomSource != HtmlDomVideo)
            m_alphaOp = AlphaDoUnmultiply;
    }
    if (!m_skiaImage)
        return false;

    m_imageSourceFormat = SK_B32_SHIFT ? DataFormatRGBA8 : DataFormatBGRA8;
    m_imageWidth = m_skiaImage->bitmap().width();
    m_imageHeight = m_skiaImage->bitmap().height();
    if (!m_imageWidth || !m_imageHeight)
        return false;
    m_imageSourceUnpackAlignment = 0;
    m_skiaImage->bitmap().lockPixels();
    m_imagePixelData = m_skiaImage->bitmap().getPixels();
    return true;
}

unsigned GraphicsContext3D::getClearBitsByFormat(GLenum format)
{
    switch (format) {
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_LUMINANCE_ALPHA:
    case GL_RGB:
    case GL_RGB565:
    case GL_RGBA:
    case GL_RGBA4:
    case GL_RGB5_A1:
        return GL_COLOR_BUFFER_BIT;
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT:
        return GL_DEPTH_BUFFER_BIT;
    case GL_STENCIL_INDEX8:
        return GL_STENCIL_BUFFER_BIT;
    case GL_DEPTH_STENCIL_OES:
        return GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    default:
        return 0;
    }
}

unsigned GraphicsContext3D::getChannelBitsByFormat(GLenum format)
{
    switch (format) {
    case GL_ALPHA:
        return ChannelAlpha;
    case GL_LUMINANCE:
        return ChannelRGB;
    case GL_LUMINANCE_ALPHA:
        return ChannelRGBA;
    case GL_RGB:
    case GL_RGB565:
        return ChannelRGB;
    case GL_RGBA:
    case GL_RGBA4:
    case GL_RGB5_A1:
        return ChannelRGBA;
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT:
        return ChannelDepth;
    case GL_STENCIL_INDEX8:
        return ChannelStencil;
    case GL_DEPTH_STENCIL_OES:
        return ChannelDepth | ChannelStencil;
    default:
        return 0;
    }
}

namespace {

void splitStringHelper(const String& str, HashSet<String>& set)
{
    Vector<String> substrings;
    str.split(" ", substrings);
    for (size_t i = 0; i < substrings.size(); ++i)
        set.add(substrings[i]);
}

String mapExtensionName(const String& name)
{
    if (name == "GL_ANGLE_framebuffer_blit"
        || name == "GL_ANGLE_framebuffer_multisample")
        return "GL_CHROMIUM_framebuffer_multisample";
    return name;
}

} // anonymous namespace

void GraphicsContext3D::initializeExtensions()
{
    if (m_initializedAvailableExtensions)
        return;

    m_initializedAvailableExtensions = true;
    bool success = m_impl->makeContextCurrent();
    ASSERT(success);
    if (!success)
        return;

    String extensionsString = m_impl->getString(GL_EXTENSIONS);
    splitStringHelper(extensionsString, m_enabledExtensions);

    String requestableExtensionsString = m_impl->getRequestableExtensionsCHROMIUM();
    splitStringHelper(requestableExtensionsString, m_requestableExtensions);
}


bool GraphicsContext3D::supportsExtension(const String& name)
{
    initializeExtensions();
    String mappedName = mapExtensionName(name);
    return m_enabledExtensions.contains(mappedName) || m_requestableExtensions.contains(mappedName);
}

bool GraphicsContext3D::ensureExtensionEnabled(const String& name)
{
    initializeExtensions();

    String mappedName = mapExtensionName(name);
    if (m_enabledExtensions.contains(mappedName))
        return true;

    if (m_requestableExtensions.contains(mappedName)) {
        m_impl->requestExtensionCHROMIUM(mappedName.ascii().data());
        m_enabledExtensions.clear();
        m_requestableExtensions.clear();
        m_initializedAvailableExtensions = false;
    }

    initializeExtensions();
    fprintf(stderr, "m_enabledExtensions.contains(%s) == %d\n", mappedName.ascii().data(), m_enabledExtensions.contains(mappedName));
    return m_enabledExtensions.contains(mappedName);
}

bool GraphicsContext3D::isExtensionEnabled(const String& name)
{
    initializeExtensions();
    String mappedName = mapExtensionName(name);
    return m_enabledExtensions.contains(mappedName);
}

bool GraphicsContext3D::canUseCopyTextureCHROMIUM(GLenum destFormat, GLenum destType, GLint level)
{
    // FIXME: restriction of (RGB || RGBA)/UNSIGNED_BYTE/(Level 0) should be lifted when
    // WebGraphicsContext3D::copyTextureCHROMIUM(...) are fully functional.
    if ((destFormat == GL_RGB || destFormat == GL_RGBA)
        && destType == GL_UNSIGNED_BYTE
        && !level)
        return true;
    return false;
}

} // namespace WebCore
