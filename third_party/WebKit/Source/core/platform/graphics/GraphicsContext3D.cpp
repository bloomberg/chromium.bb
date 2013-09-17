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

#include "core/platform/graphics/GraphicsContext3D.h"

#include "core/html/ImageData.h"
#include "core/html/canvas/CheckedInt.h"
#include "core/platform/graphics/Extensions3D.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/Image.h"
#include "core/platform/graphics/ImageBuffer.h"
#include "core/platform/graphics/ImageObserver.h"
#include "core/platform/graphics/gpu/DrawingBuffer.h"
#include "core/platform/image-decoders/ImageDecoder.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "wtf/CPU.h"
#include "wtf/OwnArrayPtr.h"
#include "wtf/PassOwnArrayPtr.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3D.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/WebGraphicsMemoryAllocation.h"

namespace {

// The limit of the number of textures we hold in the GrContext's bitmap->texture cache.
const int maxGaneshTextureCacheCount = 2048;
// The limit of the bytes allocated toward textures in the GrContext's bitmap->texture cache.
const size_t maxGaneshTextureCacheBytes = 96 * 1024 * 1024;

}

namespace WebCore {

namespace {

void getDrawingParameters(DrawingBuffer* drawingBuffer, WebKit::WebGraphicsContext3D* graphicsContext3D,
                          Platform3DObject* frameBufferId, int* width, int* height)
{
    if (drawingBuffer) {
        *frameBufferId = drawingBuffer->framebuffer();
        *width = drawingBuffer->size().width();
        *height = drawingBuffer->size().height();
    } else {
        *frameBufferId = 0;
        *width = graphicsContext3D->width();
        *height = graphicsContext3D->height();
    }
}

} // anonymous namespace

GraphicsContext3D::GraphicsContext3D(PassOwnPtr<WebKit::WebGraphicsContext3D> webContext, bool preserveDrawingBuffer)
    : m_impl(webContext.get())
    , m_ownedWebContext(webContext)
    , m_initializedAvailableExtensions(false)
    , m_layerComposited(false)
    , m_preserveDrawingBuffer(preserveDrawingBuffer)
    , m_packAlignment(4)
    , m_resourceSafety(ResourceSafetyUnknown)
    , m_grContext(0)
{
}

GraphicsContext3D::GraphicsContext3D(PassOwnPtr<WebKit::WebGraphicsContext3DProvider> provider, bool preserveDrawingBuffer)
    : m_provider(provider)
    , m_impl(m_provider->context3d())
    , m_initializedAvailableExtensions(false)
    , m_layerComposited(false)
    , m_preserveDrawingBuffer(preserveDrawingBuffer)
    , m_packAlignment(4)
    , m_resourceSafety(ResourceSafetyUnknown)
    , m_grContext(m_provider->grContext())
{
}

GraphicsContext3D::~GraphicsContext3D()
{
    setContextLostCallback(nullptr);
    setErrorMessageCallback(nullptr);

    if (m_ownedGrContext) {
        m_ownedWebContext->setMemoryAllocationChangedCallbackCHROMIUM(0);
        m_ownedGrContext->contextDestroyed();
    }
}

// Macros to assist in delegating from GraphicsContext3D to
// WebGraphicsContext3D.

#define DELEGATE_TO_WEBCONTEXT(name) \
void GraphicsContext3D::name() \
{ \
    m_impl->name(); \
}

#define DELEGATE_TO_WEBCONTEXT_R(name, rt) \
rt GraphicsContext3D::name() \
{ \
    return m_impl->name(); \
}

#define DELEGATE_TO_WEBCONTEXT_1(name, t1) \
void GraphicsContext3D::name(t1 a1) \
{ \
    m_impl->name(a1); \
}

#define DELEGATE_TO_WEBCONTEXT_1R(name, t1, rt) \
rt GraphicsContext3D::name(t1 a1) \
{ \
    return m_impl->name(a1); \
}

#define DELEGATE_TO_WEBCONTEXT_2(name, t1, t2) \
void GraphicsContext3D::name(t1 a1, t2 a2) \
{ \
    m_impl->name(a1, a2); \
}

#define DELEGATE_TO_WEBCONTEXT_2R(name, t1, t2, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2) \
{ \
    return m_impl->name(a1, a2); \
}

#define DELEGATE_TO_WEBCONTEXT_3(name, t1, t2, t3) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3) \
{ \
    m_impl->name(a1, a2, a3); \
}

#define DELEGATE_TO_WEBCONTEXT_3R(name, t1, t2, t3, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3) \
{ \
    return m_impl->name(a1, a2, a3); \
}

#define DELEGATE_TO_WEBCONTEXT_4(name, t1, t2, t3, t4) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4) \
{ \
    m_impl->name(a1, a2, a3, a4); \
}

#define DELEGATE_TO_WEBCONTEXT_4R(name, t1, t2, t3, t4, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4) \
{ \
    return m_impl->name(a1, a2, a3, a4); \
}

#define DELEGATE_TO_WEBCONTEXT_5(name, t1, t2, t3, t4, t5) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) \
{ \
    m_impl->name(a1, a2, a3, a4, a5); \
}

#define DELEGATE_TO_WEBCONTEXT_6(name, t1, t2, t3, t4, t5, t6) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    m_impl->name(a1, a2, a3, a4, a5, a6); \
}

#define DELEGATE_TO_WEBCONTEXT_6R(name, t1, t2, t3, t4, t5, t6, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    return m_impl->name(a1, a2, a3, a4, a5, a6); \
}

#define DELEGATE_TO_WEBCONTEXT_7(name, t1, t2, t3, t4, t5, t6, t7) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{ \
    m_impl->name(a1, a2, a3, a4, a5, a6, a7); \
}

#define DELEGATE_TO_WEBCONTEXT_7R(name, t1, t2, t3, t4, t5, t6, t7, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{ \
    return m_impl->name(a1, a2, a3, a4, a5, a6, a7); \
}

#define DELEGATE_TO_WEBCONTEXT_8(name, t1, t2, t3, t4, t5, t6, t7, t8) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8) \
{ \
    m_impl->name(a1, a2, a3, a4, a5, a6, a7, a8); \
}

#define DELEGATE_TO_WEBCONTEXT_9(name, t1, t2, t3, t4, t5, t6, t7, t8, t9) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) \
{ \
    m_impl->name(a1, a2, a3, a4, a5, a6, a7, a8, a9); \
}

#define DELEGATE_TO_WEBCONTEXT_9R(name, t1, t2, t3, t4, t5, t6, t7, t8, t9, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) \
{ \
    return m_impl->name(a1, a2, a3, a4, a5, a6, a7, a8, a9); \
}

class GraphicsContext3DContextLostCallbackAdapter : public WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback {
public:
    GraphicsContext3DContextLostCallbackAdapter(PassOwnPtr<GraphicsContext3D::ContextLostCallback> callback)
        : m_contextLostCallback(callback) { }
    virtual ~GraphicsContext3DContextLostCallbackAdapter() { }

    virtual void onContextLost()
    {
        if (m_contextLostCallback)
            m_contextLostCallback->onContextLost();
    }
private:
    OwnPtr<GraphicsContext3D::ContextLostCallback> m_contextLostCallback;
};

class GraphicsContext3DErrorMessageCallbackAdapter : public WebKit::WebGraphicsContext3D::WebGraphicsErrorMessageCallback {
public:
    GraphicsContext3DErrorMessageCallbackAdapter(PassOwnPtr<GraphicsContext3D::ErrorMessageCallback> callback)
        : m_errorMessageCallback(callback) { }
    virtual ~GraphicsContext3DErrorMessageCallbackAdapter() { }

    virtual void onErrorMessage(const WebKit::WebString& message, WebKit::WGC3Dint id)
    {
        if (m_errorMessageCallback)
            m_errorMessageCallback->onErrorMessage(message, id);
    }
private:
    OwnPtr<GraphicsContext3D::ErrorMessageCallback> m_errorMessageCallback;
};

void GraphicsContext3D::setContextLostCallback(PassOwnPtr<GraphicsContext3D::ContextLostCallback> callback)
{
    if (m_ownedWebContext) {
        m_contextLostCallbackAdapter = adoptPtr(new GraphicsContext3DContextLostCallbackAdapter(callback));
        m_ownedWebContext->setContextLostCallback(m_contextLostCallbackAdapter.get());
    }
}

void GraphicsContext3D::setErrorMessageCallback(PassOwnPtr<GraphicsContext3D::ErrorMessageCallback> callback)
{
    if (m_ownedWebContext) {
        m_errorMessageCallbackAdapter = adoptPtr(new GraphicsContext3DErrorMessageCallbackAdapter(callback));
        m_ownedWebContext->setErrorMessageCallback(m_errorMessageCallbackAdapter.get());
    }
}

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attrs)
{
    WebKit::WebGraphicsContext3D::Attributes webAttributes;
    webAttributes.alpha = attrs.alpha;
    webAttributes.depth = attrs.depth;
    webAttributes.stencil = attrs.stencil;
    webAttributes.antialias = attrs.antialias;
    webAttributes.premultipliedAlpha = attrs.premultipliedAlpha;
    webAttributes.noExtensions = attrs.noExtensions;
    webAttributes.shareResources = attrs.shareResources;
    webAttributes.preferDiscreteGPU = attrs.preferDiscreteGPU;
    webAttributes.topDocumentURL = attrs.topDocumentURL.string();

    OwnPtr<WebKit::WebGraphicsContext3D> webContext = adoptPtr(WebKit::Platform::current()->createOffscreenGraphicsContext3D(webAttributes));
    if (!webContext)
        return 0;

    return GraphicsContext3D::createGraphicsContextFromWebContext(webContext.release(), attrs.preserveDrawingBuffer);
}

PassRefPtr<GraphicsContext3D> GraphicsContext3D::createGraphicsContextFromProvider(PassOwnPtr<WebKit::WebGraphicsContext3DProvider> provider, bool preserveDrawingBuffer)
{
    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(provider, preserveDrawingBuffer));
    return context.release();
}

PassRefPtr<GraphicsContext3D> GraphicsContext3D::createGraphicsContextFromWebContext(PassOwnPtr<WebKit::WebGraphicsContext3D> webContext, bool preserveDrawingBuffer)
{
    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(webContext, preserveDrawingBuffer));
    return context.release();
}

class GrMemoryAllocationChangedCallbackAdapter : public WebKit::WebGraphicsContext3D::WebGraphicsMemoryAllocationChangedCallbackCHROMIUM {
public:
    GrMemoryAllocationChangedCallbackAdapter(GrContext* context)
        : m_context(context)
    {
    }

    virtual void onMemoryAllocationChanged(WebKit::WebGraphicsMemoryAllocation allocation) OVERRIDE
    {
        if (!m_context)
            return;

        if (!allocation.gpuResourceSizeInBytes) {
            m_context->freeGpuResources();
            m_context->setTextureCacheLimits(0, 0);
        } else
            m_context->setTextureCacheLimits(maxGaneshTextureCacheCount, maxGaneshTextureCacheBytes);
    }

private:
    GrContext* m_context;
};

namespace {
void bindWebGraphicsContext3DGLContextCallback(const GrGLInterface* interface)
{
    reinterpret_cast<WebKit::WebGraphicsContext3D*>(interface->fCallbackData)->makeContextCurrent();
}
}

GrContext* GraphicsContext3D::grContext()
{
    if (m_grContext)
        return m_grContext;
    if (!m_ownedWebContext)
        return 0;

    SkAutoTUnref<GrGLInterface> interface(m_ownedWebContext->createGrGLInterface());
    if (!interface)
        return 0;

    interface->fCallback = bindWebGraphicsContext3DGLContextCallback;
    interface->fCallbackData = reinterpret_cast<GrGLInterfaceCallbackData>(m_ownedWebContext.get());

    m_ownedGrContext.reset(GrContext::Create(kOpenGL_GrBackend, reinterpret_cast<GrBackendContext>(interface.get())));
    m_grContext = m_ownedGrContext;
    if (!m_grContext)
        return 0;

    m_grContext->setTextureCacheLimits(maxGaneshTextureCacheCount, maxGaneshTextureCacheBytes);
    m_grContextMemoryAllocationCallbackAdapter = adoptPtr(new GrMemoryAllocationChangedCallbackAdapter(m_grContext));
    m_ownedWebContext->setMemoryAllocationChangedCallbackCHROMIUM(m_grContextMemoryAllocationCallbackAdapter.get());

    return m_grContext;
}

DELEGATE_TO_WEBCONTEXT_R(makeContextCurrent, bool)
DELEGATE_TO_WEBCONTEXT_R(lastFlushID, uint32_t)

DELEGATE_TO_WEBCONTEXT_1(activeTexture, GC3Denum)
DELEGATE_TO_WEBCONTEXT_2(attachShader, Platform3DObject, Platform3DObject)

void GraphicsContext3D::bindAttribLocation(Platform3DObject program, GC3Duint index, const String& name)
{
    m_impl->bindAttribLocation(program, index, name.utf8().data());
}

DELEGATE_TO_WEBCONTEXT_2(bindBuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_2(bindFramebuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_2(bindRenderbuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_2(bindTexture, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_4(blendColor, GC3Dclampf, GC3Dclampf, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_WEBCONTEXT_1(blendEquation, GC3Denum)
DELEGATE_TO_WEBCONTEXT_2(blendEquationSeparate, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_2(blendFunc, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_4(blendFuncSeparate, GC3Denum, GC3Denum, GC3Denum, GC3Denum)

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, GC3Denum usage)
{
    bufferData(target, size, 0, usage);
}

DELEGATE_TO_WEBCONTEXT_4(bufferData, GC3Denum, GC3Dsizeiptr, const void*, GC3Denum)
DELEGATE_TO_WEBCONTEXT_4(bufferSubData, GC3Denum, GC3Dintptr, GC3Dsizeiptr, const void*)

DELEGATE_TO_WEBCONTEXT_1R(checkFramebufferStatus, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(clear, GC3Dbitfield)
DELEGATE_TO_WEBCONTEXT_4(clearColor, GC3Dclampf, GC3Dclampf, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_WEBCONTEXT_1(clearDepth, GC3Dclampf)
DELEGATE_TO_WEBCONTEXT_1(clearStencil, GC3Dint)
DELEGATE_TO_WEBCONTEXT_4(colorMask, GC3Dboolean, GC3Dboolean, GC3Dboolean, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1(compileShader, Platform3DObject)

DELEGATE_TO_WEBCONTEXT_8(compressedTexImage2D, GC3Denum, GC3Dint, GC3Denum, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, const void*)
DELEGATE_TO_WEBCONTEXT_9(compressedTexSubImage2D, GC3Denum, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Denum, GC3Dsizei, const void*)
DELEGATE_TO_WEBCONTEXT_8(copyTexImage2D, GC3Denum, GC3Dint, GC3Denum, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Dint)
DELEGATE_TO_WEBCONTEXT_8(copyTexSubImage2D, GC3Denum, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)
DELEGATE_TO_WEBCONTEXT_1(cullFace, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(depthFunc, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(depthMask, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_2(depthRange, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_WEBCONTEXT_2(detachShader, Platform3DObject, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(disable, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(disableVertexAttribArray, GC3Duint)
DELEGATE_TO_WEBCONTEXT_3(drawArrays, GC3Denum, GC3Dint, GC3Dsizei)
DELEGATE_TO_WEBCONTEXT_4(drawElements, GC3Denum, GC3Dsizei, GC3Denum, GC3Dintptr)

DELEGATE_TO_WEBCONTEXT_1(enable, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(enableVertexAttribArray, GC3Duint)
DELEGATE_TO_WEBCONTEXT(finish)
DELEGATE_TO_WEBCONTEXT(flush)
DELEGATE_TO_WEBCONTEXT_4(framebufferRenderbuffer, GC3Denum, GC3Denum, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_5(framebufferTexture2D, GC3Denum, GC3Denum, GC3Denum, Platform3DObject, GC3Dint)
DELEGATE_TO_WEBCONTEXT_1(frontFace, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(generateMipmap, GC3Denum)

bool GraphicsContext3D::getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    WebKit::WebGraphicsContext3D::ActiveInfo webInfo;
    if (!m_impl->getActiveAttrib(program, index, webInfo))
        return false;
    info.name = webInfo.name;
    info.type = webInfo.type;
    info.size = webInfo.size;
    return true;
}

bool GraphicsContext3D::getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    WebKit::WebGraphicsContext3D::ActiveInfo webInfo;
    if (!m_impl->getActiveUniform(program, index, webInfo))
        return false;
    info.name = webInfo.name;
    info.type = webInfo.type;
    info.size = webInfo.size;
    return true;
}

DELEGATE_TO_WEBCONTEXT_4(getAttachedShaders, Platform3DObject, GC3Dsizei, GC3Dsizei*, Platform3DObject*)

GC3Dint GraphicsContext3D::getAttribLocation(Platform3DObject program, const String& name)
{
    return m_impl->getAttribLocation(program, name.utf8().data());
}

DELEGATE_TO_WEBCONTEXT_2(getBooleanv, GC3Denum, GC3Dboolean*)
DELEGATE_TO_WEBCONTEXT_3(getBufferParameteriv, GC3Denum, GC3Denum, GC3Dint*)

GraphicsContext3D::Attributes GraphicsContext3D::getContextAttributes()
{
    WebKit::WebGraphicsContext3D::Attributes webAttributes = m_impl->getContextAttributes();
    GraphicsContext3D::Attributes attributes;
    attributes.alpha = webAttributes.alpha;
    attributes.depth = webAttributes.depth;
    attributes.stencil = webAttributes.stencil;
    attributes.antialias = webAttributes.antialias;
    attributes.premultipliedAlpha = webAttributes.premultipliedAlpha;
    attributes.preserveDrawingBuffer = m_preserveDrawingBuffer;
    attributes.preferDiscreteGPU = webAttributes.preferDiscreteGPU;
    return attributes;
}

DELEGATE_TO_WEBCONTEXT_R(getError, GC3Denum)
DELEGATE_TO_WEBCONTEXT_2(getFloatv, GC3Denum, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(getFramebufferAttachmentParameteriv, GC3Denum, GC3Denum, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_2(getIntegerv, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_3(getProgramiv, Platform3DObject, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_1R(getProgramInfoLog, Platform3DObject, String)
DELEGATE_TO_WEBCONTEXT_3(getRenderbufferParameteriv, GC3Denum, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_3(getShaderiv, Platform3DObject, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_1R(getShaderInfoLog, Platform3DObject, String)
DELEGATE_TO_WEBCONTEXT_4(getShaderPrecisionFormat, GC3Denum, GC3Denum, GC3Dint*, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_1R(getShaderSource, Platform3DObject, String)
DELEGATE_TO_WEBCONTEXT_1R(getString, GC3Denum, String)
DELEGATE_TO_WEBCONTEXT_3(getTexParameterfv, GC3Denum, GC3Denum, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(getTexParameteriv, GC3Denum, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_3(getUniformfv, Platform3DObject, GC3Dint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(getUniformiv, Platform3DObject, GC3Dint, GC3Dint*)

GC3Dint GraphicsContext3D::getUniformLocation(Platform3DObject program, const String& name)
{
    return m_impl->getUniformLocation(program, name.utf8().data());
}

DELEGATE_TO_WEBCONTEXT_3(getVertexAttribfv, GC3Duint, GC3Denum, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(getVertexAttribiv, GC3Duint, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_2R(getVertexAttribOffset, GC3Duint, GC3Denum, GC3Dsizeiptr)

DELEGATE_TO_WEBCONTEXT_2(hint, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1R(isBuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isEnabled, GC3Denum, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isFramebuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isProgram, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isRenderbuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isShader, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isTexture, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1(lineWidth, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_1(linkProgram, Platform3DObject)

void GraphicsContext3D::pixelStorei(GC3Denum pname, GC3Dint param)
{
    if (pname == PACK_ALIGNMENT)
        m_packAlignment = param;
    m_impl->pixelStorei(pname, param);
}

DELEGATE_TO_WEBCONTEXT_2(polygonOffset, GC3Dfloat, GC3Dfloat)

DELEGATE_TO_WEBCONTEXT_7(readPixels, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Denum, GC3Denum, void*)

DELEGATE_TO_WEBCONTEXT(releaseShaderCompiler)
DELEGATE_TO_WEBCONTEXT_4(renderbufferStorage, GC3Denum, GC3Denum, GC3Dsizei, GC3Dsizei)
DELEGATE_TO_WEBCONTEXT_2(sampleCoverage, GC3Dclampf, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_4(scissor, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)

void GraphicsContext3D::shaderSource(Platform3DObject shader, const String& string)
{
    m_impl->shaderSource(shader, string.utf8().data());
}

DELEGATE_TO_WEBCONTEXT_3(stencilFunc, GC3Denum, GC3Dint, GC3Duint)
DELEGATE_TO_WEBCONTEXT_4(stencilFuncSeparate, GC3Denum, GC3Denum, GC3Dint, GC3Duint)
DELEGATE_TO_WEBCONTEXT_1(stencilMask, GC3Duint)
DELEGATE_TO_WEBCONTEXT_2(stencilMaskSeparate, GC3Denum, GC3Duint)
DELEGATE_TO_WEBCONTEXT_3(stencilOp, GC3Denum, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_4(stencilOpSeparate, GC3Denum, GC3Denum, GC3Denum, GC3Denum)

DELEGATE_TO_WEBCONTEXT_9(texImage2D, GC3Denum, GC3Dint, GC3Denum, GC3Dsizei, GC3Dsizei, GC3Dint, GC3Denum, GC3Denum, const void*)
DELEGATE_TO_WEBCONTEXT_3(texParameterf, GC3Denum, GC3Denum, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(texParameteri, GC3Denum, GC3Denum, GC3Dint)
DELEGATE_TO_WEBCONTEXT_9(texSubImage2D, GC3Denum, GC3Dint, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Denum, GC3Denum, const void*)

DELEGATE_TO_WEBCONTEXT_2(uniform1f, GC3Dint, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(uniform1fv, GC3Dint, GC3Dsizei, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_2(uniform1i, GC3Dint, GC3Dint)
DELEGATE_TO_WEBCONTEXT_3(uniform1iv, GC3Dint, GC3Dsizei, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_3(uniform2f, GC3Dint, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(uniform2fv, GC3Dint, GC3Dsizei, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(uniform2i, GC3Dint, GC3Dint, GC3Dint)
DELEGATE_TO_WEBCONTEXT_3(uniform2iv, GC3Dint, GC3Dsizei, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_4(uniform3f, GC3Dint, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(uniform3fv, GC3Dint, GC3Dsizei, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(uniform3i, GC3Dint, GC3Dint, GC3Dint, GC3Dint)
DELEGATE_TO_WEBCONTEXT_3(uniform3iv, GC3Dint, GC3Dsizei, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_5(uniform4f, GC3Dint, GC3Dfloat, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(uniform4fv, GC3Dint, GC3Dsizei, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_5(uniform4i, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint)
DELEGATE_TO_WEBCONTEXT_3(uniform4iv, GC3Dint, GC3Dsizei, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_4(uniformMatrix2fv, GC3Dint, GC3Dsizei, GC3Dboolean, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(uniformMatrix3fv, GC3Dint, GC3Dsizei, GC3Dboolean, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(uniformMatrix4fv, GC3Dint, GC3Dsizei, GC3Dboolean, GC3Dfloat*)

DELEGATE_TO_WEBCONTEXT_1(useProgram, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(validateProgram, Platform3DObject)

DELEGATE_TO_WEBCONTEXT_2(vertexAttrib1f, GC3Duint, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_2(vertexAttrib1fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(vertexAttrib2f, GC3Duint, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_2(vertexAttrib2fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(vertexAttrib3f, GC3Duint, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_2(vertexAttrib3fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_5(vertexAttrib4f, GC3Duint, GC3Dfloat, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_2(vertexAttrib4fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_6(vertexAttribPointer, GC3Duint, GC3Dint, GC3Denum, GC3Dboolean, GC3Dsizei, GC3Dintptr)

DELEGATE_TO_WEBCONTEXT_4(viewport, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)

void GraphicsContext3D::reshape(int width, int height)
{
    if (width == m_impl->width() && height == m_impl->height())
        return;

    m_impl->reshape(width, height);
}

void GraphicsContext3D::markContextChanged()
{
    m_layerComposited = false;
}

bool GraphicsContext3D::layerComposited() const
{
    return m_layerComposited;
}

void GraphicsContext3D::markLayerComposited()
{
    m_layerComposited = true;
}

void GraphicsContext3D::paintRenderingResultsToCanvas(ImageBuffer* imageBuffer, DrawingBuffer* drawingBuffer)
{
    Platform3DObject framebufferId;
    int width, height;
    getDrawingParameters(drawingBuffer, m_impl, &framebufferId, &width, &height);
    paintFramebufferToCanvas(framebufferId, width, height, !getContextAttributes().premultipliedAlpha, imageBuffer);
}

PassRefPtr<ImageData> GraphicsContext3D::paintRenderingResultsToImageData(DrawingBuffer* drawingBuffer)
{
    if (getContextAttributes().premultipliedAlpha)
        return 0;

    Platform3DObject framebufferId;
    int width, height;
    getDrawingParameters(drawingBuffer, m_impl, &framebufferId, &width, &height);

    RefPtr<ImageData> imageData = ImageData::create(IntSize(width, height));
    unsigned char* pixels = imageData->data()->data();

    m_impl->bindFramebuffer(FRAMEBUFFER, framebufferId);
    readBackFramebuffer(pixels, width, height, ReadbackRGBA, AlphaDoNothing);
    flipVertically(pixels, width, height);

    return imageData.release();
}

void GraphicsContext3D::readBackFramebuffer(unsigned char* pixels, int width, int height, ReadbackOrder readbackOrder, AlphaOp op)
{
    if (m_packAlignment > 4)
        m_impl->pixelStorei(PACK_ALIGNMENT, 1);
    m_impl->readPixels(0, 0, width, height, RGBA, UNSIGNED_BYTE, pixels);
    if (m_packAlignment > 4)
        m_impl->pixelStorei(PACK_ALIGNMENT, m_packAlignment);

    size_t bufferSize = 4 * width * height;

    if (readbackOrder == ReadbackSkia) {
#if (SK_R32_SHIFT == 16) && !SK_B32_SHIFT
        // Swizzle red and blue channels to match SkBitmap's byte ordering.
        // TODO(kbr): expose GL_BGRA as extension.
        for (size_t i = 0; i < bufferSize; i += 4) {
            std::swap(pixels[i], pixels[i + 2]);
        }
#endif
    }

    if (op == AlphaDoPremultiply) {
        for (size_t i = 0; i < bufferSize; i += 4) {
            pixels[i + 0] = std::min(255, pixels[i + 0] * pixels[i + 3] / 255);
            pixels[i + 1] = std::min(255, pixels[i + 1] * pixels[i + 3] / 255);
            pixels[i + 2] = std::min(255, pixels[i + 2] * pixels[i + 3] / 255);
        }
    } else if (op != AlphaDoNothing) {
        ASSERT_NOT_REACHED();
    }
}

DELEGATE_TO_WEBCONTEXT_R(createBuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_R(createFramebuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_R(createProgram, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_R(createRenderbuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1R(createShader, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_R(createTexture, Platform3DObject)

DELEGATE_TO_WEBCONTEXT_1(deleteBuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteFramebuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteProgram, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteRenderbuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteShader, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteTexture, Platform3DObject)

DELEGATE_TO_WEBCONTEXT_1(synthesizeGLError, GC3Denum)

Extensions3D* GraphicsContext3D::getExtensions()
{
    if (!m_extensions)
        m_extensions = adoptPtr(new Extensions3D(this));
    return m_extensions.get();
}

bool GraphicsContext3D::texImage2DResourceSafe(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, GC3Dint unpackAlignment)
{
    ASSERT(unpackAlignment == 1 || unpackAlignment == 2 || unpackAlignment == 4 || unpackAlignment == 8);
    texImage2D(target, level, internalformat, width, height, border, format, type, 0);
    return true;
}

bool GraphicsContext3D::computeFormatAndTypeParameters(GC3Denum format,
                                                       GC3Denum type,
                                                       unsigned int* componentsPerPixel,
                                                       unsigned int* bytesPerComponent)
{
    switch (format) {
    case GraphicsContext3D::ALPHA:
    case GraphicsContext3D::LUMINANCE:
    case GraphicsContext3D::DEPTH_COMPONENT:
    case GraphicsContext3D::DEPTH_STENCIL:
        *componentsPerPixel = 1;
        break;
    case GraphicsContext3D::LUMINANCE_ALPHA:
        *componentsPerPixel = 2;
        break;
    case GraphicsContext3D::RGB:
        *componentsPerPixel = 3;
        break;
    case GraphicsContext3D::RGBA:
    case Extensions3D::BGRA_EXT: // GL_EXT_texture_format_BGRA8888
        *componentsPerPixel = 4;
        break;
    default:
        return false;
    }
    switch (type) {
    case GraphicsContext3D::UNSIGNED_BYTE:
        *bytesPerComponent = sizeof(GC3Dubyte);
        break;
    case GraphicsContext3D::UNSIGNED_SHORT:
        *bytesPerComponent = sizeof(GC3Dushort);
        break;
    case GraphicsContext3D::UNSIGNED_SHORT_5_6_5:
    case GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1:
        *componentsPerPixel = 1;
        *bytesPerComponent = sizeof(GC3Dushort);
        break;
    case GraphicsContext3D::UNSIGNED_INT_24_8:
    case GraphicsContext3D::UNSIGNED_INT:
        *bytesPerComponent = sizeof(GC3Duint);
        break;
    case GraphicsContext3D::FLOAT: // OES_texture_float
        *bytesPerComponent = sizeof(GC3Dfloat);
        break;
    case GraphicsContext3D::HALF_FLOAT_OES: // OES_texture_half_float
        *bytesPerComponent = sizeof(GC3Dhalffloat);
        break;
    default:
        return false;
    }
    return true;
}

GC3Denum GraphicsContext3D::computeImageSizeInBytes(GC3Denum format, GC3Denum type, GC3Dsizei width, GC3Dsizei height, GC3Dint alignment,
                                                    unsigned int* imageSizeInBytes, unsigned int* paddingInBytes)
{
    ASSERT(imageSizeInBytes);
    ASSERT(alignment == 1 || alignment == 2 || alignment == 4 || alignment == 8);
    if (width < 0 || height < 0)
        return GraphicsContext3D::INVALID_VALUE;
    unsigned int bytesPerComponent, componentsPerPixel;
    if (!computeFormatAndTypeParameters(format, type, &bytesPerComponent, &componentsPerPixel))
        return GraphicsContext3D::INVALID_ENUM;
    if (!width || !height) {
        *imageSizeInBytes = 0;
        if (paddingInBytes)
            *paddingInBytes = 0;
        return GraphicsContext3D::NO_ERROR;
    }
    CheckedInt<uint32_t> checkedValue(bytesPerComponent * componentsPerPixel);
    checkedValue *=  width;
    if (!checkedValue.isValid())
        return GraphicsContext3D::INVALID_VALUE;
    unsigned int validRowSize = checkedValue.value();
    unsigned int padding = 0;
    unsigned int residual = validRowSize % alignment;
    if (residual) {
        padding = alignment - residual;
        checkedValue += padding;
    }
    // Last row needs no padding.
    checkedValue *= (height - 1);
    checkedValue += validRowSize;
    if (!checkedValue.isValid())
        return GraphicsContext3D::INVALID_VALUE;
    *imageSizeInBytes = checkedValue.value();
    if (paddingInBytes)
        *paddingInBytes = padding;
    return GraphicsContext3D::NO_ERROR;
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

unsigned GraphicsContext3D::getClearBitsByFormat(GC3Denum format)
{
    switch (format) {
    case GraphicsContext3D::ALPHA:
    case GraphicsContext3D::LUMINANCE:
    case GraphicsContext3D::LUMINANCE_ALPHA:
    case GraphicsContext3D::RGB:
    case GraphicsContext3D::RGB565:
    case GraphicsContext3D::RGBA:
    case GraphicsContext3D::RGBA4:
    case GraphicsContext3D::RGB5_A1:
        return GraphicsContext3D::COLOR_BUFFER_BIT;
    case GraphicsContext3D::DEPTH_COMPONENT16:
    case GraphicsContext3D::DEPTH_COMPONENT:
        return GraphicsContext3D::DEPTH_BUFFER_BIT;
    case GraphicsContext3D::STENCIL_INDEX8:
        return GraphicsContext3D::STENCIL_BUFFER_BIT;
    case GraphicsContext3D::DEPTH_STENCIL:
        return GraphicsContext3D::DEPTH_BUFFER_BIT | GraphicsContext3D::STENCIL_BUFFER_BIT;
    default:
        return 0;
    }
}

unsigned GraphicsContext3D::getChannelBitsByFormat(GC3Denum format)
{
    switch (format) {
    case GraphicsContext3D::ALPHA:
        return ChannelAlpha;
    case GraphicsContext3D::LUMINANCE:
        return ChannelRGB;
    case GraphicsContext3D::LUMINANCE_ALPHA:
        return ChannelRGBA;
    case GraphicsContext3D::RGB:
    case GraphicsContext3D::RGB565:
        return ChannelRGB;
    case GraphicsContext3D::RGBA:
    case GraphicsContext3D::RGBA4:
    case GraphicsContext3D::RGB5_A1:
        return ChannelRGBA;
    case GraphicsContext3D::DEPTH_COMPONENT16:
    case GraphicsContext3D::DEPTH_COMPONENT:
        return ChannelDepth;
    case GraphicsContext3D::STENCIL_INDEX8:
        return ChannelStencil;
    case GraphicsContext3D::DEPTH_STENCIL:
        return ChannelDepth | ChannelStencil;
    default:
        return 0;
    }
}

void GraphicsContext3D::paintFramebufferToCanvas(int framebuffer, int width, int height, bool premultiplyAlpha, ImageBuffer* imageBuffer)
{
    unsigned char* pixels = 0;

    const SkBitmap* canvasBitmap = imageBuffer->context()->bitmap();
    const SkBitmap* readbackBitmap = 0;
    ASSERT(canvasBitmap->config() == SkBitmap::kARGB_8888_Config);
    if (canvasBitmap->width() == width && canvasBitmap->height() == height) {
        // This is the fastest and most common case. We read back
        // directly into the canvas's backing store.
        readbackBitmap = canvasBitmap;
        m_resizingBitmap.reset();
    } else {
        // We need to allocate a temporary bitmap for reading back the
        // pixel data. We will then use Skia to rescale this bitmap to
        // the size of the canvas's backing store.
        if (m_resizingBitmap.width() != width || m_resizingBitmap.height() != height) {
            m_resizingBitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
            if (!m_resizingBitmap.allocPixels())
                return;
        }
        readbackBitmap = &m_resizingBitmap;
    }

    // Read back the frame buffer.
    SkAutoLockPixels bitmapLock(*readbackBitmap);
    pixels = static_cast<unsigned char*>(readbackBitmap->getPixels());

    m_impl->bindFramebuffer(FRAMEBUFFER, framebuffer);
    readBackFramebuffer(pixels, width, height, ReadbackSkia, premultiplyAlpha ? AlphaDoPremultiply : AlphaDoNothing);
    flipVertically(pixels, width, height);

    readbackBitmap->notifyPixelsChanged();
    if (m_resizingBitmap.readyToDraw()) {
        // We need to draw the resizing bitmap into the canvas's backing store.
        SkCanvas canvas(*canvasBitmap);
        SkRect dst;
        dst.set(SkIntToScalar(0), SkIntToScalar(0), SkIntToScalar(canvasBitmap->width()), SkIntToScalar(canvasBitmap->height()));
        canvas.drawBitmapRect(m_resizingBitmap, 0, dst);
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

    String extensionsString = m_impl->getString(GraphicsContext3D::EXTENSIONS);
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

void GraphicsContext3D::flipVertically(uint8_t* framebuffer, int width, int height)
{
    m_scanline.resize(width * 4);
    uint8* scanline = &m_scanline[0];
    unsigned rowBytes = width * 4;
    unsigned count = height / 2;
    for (unsigned i = 0; i < count; i++) {
        uint8* rowA = framebuffer + i * rowBytes;
        uint8* rowB = framebuffer + (height - i - 1) * rowBytes;
        memcpy(scanline, rowB, rowBytes);
        memcpy(rowB, rowA, rowBytes);
        memcpy(rowA, scanline, rowBytes);
    }
}

} // namespace WebCore
