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

#ifndef WebGLRenderingContext_h
#define WebGLRenderingContext_h

#include "core/dom/ActiveDOMObject.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/WebGLGetInfo.h"
#include "core/page/Page.h"
#include "core/platform/Timer.h"
#include "core/platform/graphics/GraphicsContext3D.h"
#include "core/platform/graphics/ImageBuffer.h"

#include "wtf/Float32Array.h"
#include "wtf/Int32Array.h"
#include "wtf/OwnArrayPtr.h"
#include "wtf/text/WTFString.h"

namespace WebKit { class WebLayer; }

namespace WebCore {

class ANGLEInstancedArrays;
class DrawingBuffer;
class EXTFragDepth;
class EXTTextureFilterAnisotropic;
class ExceptionState;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBuffer;
class ImageData;
class IntSize;
class OESElementIndexUint;
class OESStandardDerivatives;
class OESTextureFloat;
class OESTextureFloatLinear;
class OESTextureHalfFloat;
class OESTextureHalfFloatLinear;
class OESVertexArrayObject;
class WebGLActiveInfo;
class WebGLBuffer;
class WebGLCompressedTextureATC;
class WebGLCompressedTexturePVRTC;
class WebGLCompressedTextureS3TC;
class WebGLContextAttributes;
class WebGLContextGroup;
class WebGLContextObject;
class WebGLDebugRendererInfo;
class WebGLDebugShaders;
class WebGLDepthTexture;
class WebGLDrawBuffers;
class WebGLExtension;
class WebGLFramebuffer;
class WebGLLoseContext;
class WebGLObject;
class WebGLProgram;
class WebGLRenderbuffer;
class WebGLShader;
class WebGLShaderPrecisionFormat;
class WebGLSharedObject;
class WebGLTexture;
class WebGLUniformLocation;
class WebGLVertexArrayObjectOES;

class WebGLRenderingContext : public CanvasRenderingContext, public ActiveDOMObject, private Page::MultisamplingChangedObserver {
public:
    static PassOwnPtr<WebGLRenderingContext> create(HTMLCanvasElement*, WebGLContextAttributes*);
    virtual ~WebGLRenderingContext();

    virtual bool is3d() const { return true; }
    virtual bool isAccelerated() const { return true; }

    int drawingBufferWidth() const;
    int drawingBufferHeight() const;

    void activeTexture(GC3Denum texture);
    void attachShader(WebGLProgram*, WebGLShader*);
    void bindAttribLocation(WebGLProgram*, GC3Duint index, const String& name);
    void bindBuffer(GC3Denum target, WebGLBuffer*);
    void bindFramebuffer(GC3Denum target, WebGLFramebuffer*);
    void bindRenderbuffer(GC3Denum target, WebGLRenderbuffer*);
    void bindTexture(GC3Denum target, WebGLTexture*);
    void blendColor(GC3Dfloat red, GC3Dfloat green, GC3Dfloat blue, GC3Dfloat alpha);
    void blendEquation(GC3Denum mode);
    void blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha);
    void blendFunc(GC3Denum sfactor, GC3Denum dfactor);
    void blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha);

    void bufferData(GC3Denum target, long long size, GC3Denum usage);
    void bufferData(GC3Denum target, ArrayBuffer* data, GC3Denum usage);
    void bufferData(GC3Denum target, ArrayBufferView* data, GC3Denum usage);
    void bufferSubData(GC3Denum target, long long offset, ArrayBuffer* data);
    void bufferSubData(GC3Denum target, long long offset, ArrayBufferView* data);

    GC3Denum checkFramebufferStatus(GC3Denum target);
    void clear(GC3Dbitfield mask);
    void clearColor(GC3Dfloat red, GC3Dfloat green, GC3Dfloat blue, GC3Dfloat alpha);
    void clearDepth(GC3Dfloat);
    void clearStencil(GC3Dint);
    void colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha);
    void compileShader(WebGLShader*);

    void compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width,
                              GC3Dsizei height, GC3Dint border, ArrayBufferView* data);
    void compressedTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
                                 GC3Dsizei width, GC3Dsizei height, GC3Denum format, ArrayBufferView* data);

    void copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border);
    void copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);

    PassRefPtr<WebGLBuffer> createBuffer();
    PassRefPtr<WebGLFramebuffer> createFramebuffer();
    PassRefPtr<WebGLProgram> createProgram();
    PassRefPtr<WebGLRenderbuffer> createRenderbuffer();
    PassRefPtr<WebGLShader> createShader(GC3Denum type);
    PassRefPtr<WebGLTexture> createTexture();

    void cullFace(GC3Denum mode);

    void deleteBuffer(WebGLBuffer*);
    void deleteFramebuffer(WebGLFramebuffer*);
    void deleteProgram(WebGLProgram*);
    void deleteRenderbuffer(WebGLRenderbuffer*);
    void deleteShader(WebGLShader*);
    void deleteTexture(WebGLTexture*);

    void depthFunc(GC3Denum);
    void depthMask(GC3Dboolean);
    void depthRange(GC3Dfloat zNear, GC3Dfloat zFar);
    void detachShader(WebGLProgram*, WebGLShader*);
    void disable(GC3Denum cap);
    void disableVertexAttribArray(GC3Duint index);
    void drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count);
    void drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, long long offset);

    void drawArraysInstancedANGLE(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei primcount);
    void drawElementsInstancedANGLE(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset, GC3Dsizei primcount);

    void enable(GC3Denum cap);
    void enableVertexAttribArray(GC3Duint index);
    void finish();
    void flush();
    void framebufferRenderbuffer(GC3Denum target, GC3Denum attachment, GC3Denum renderbuffertarget, WebGLRenderbuffer*);
    void framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum textarget, WebGLTexture*, GC3Dint level);
    void frontFace(GC3Denum mode);
    void generateMipmap(GC3Denum target);

    PassRefPtr<WebGLActiveInfo> getActiveAttrib(WebGLProgram*, GC3Duint index);
    PassRefPtr<WebGLActiveInfo> getActiveUniform(WebGLProgram*, GC3Duint index);
    bool getAttachedShaders(WebGLProgram*, Vector<RefPtr<WebGLShader> >&);
    GC3Dint getAttribLocation(WebGLProgram*, const String& name);
    WebGLGetInfo getBufferParameter(GC3Denum target, GC3Denum pname);
    PassRefPtr<WebGLContextAttributes> getContextAttributes();
    GC3Denum getError();
    PassRefPtr<WebGLExtension> getExtension(const String& name);
    WebGLGetInfo getFramebufferAttachmentParameter(GC3Denum target, GC3Denum attachment, GC3Denum pname);
    WebGLGetInfo getParameter(GC3Denum pname);
    WebGLGetInfo getProgramParameter(WebGLProgram*, GC3Denum pname);
    String getProgramInfoLog(WebGLProgram*);
    WebGLGetInfo getRenderbufferParameter(GC3Denum target, GC3Denum pname);
    WebGLGetInfo getShaderParameter(WebGLShader*, GC3Denum pname);
    String getShaderInfoLog(WebGLShader*);
    PassRefPtr<WebGLShaderPrecisionFormat> getShaderPrecisionFormat(GC3Denum shaderType, GC3Denum precisionType);
    String getShaderSource(WebGLShader*);
    Vector<String> getSupportedExtensions();
    WebGLGetInfo getTexParameter(GC3Denum target, GC3Denum pname);
    WebGLGetInfo getUniform(WebGLProgram*, const WebGLUniformLocation*);
    PassRefPtr<WebGLUniformLocation> getUniformLocation(WebGLProgram*, const String&);
    WebGLGetInfo getVertexAttrib(GC3Duint index, GC3Denum pname);
    long long getVertexAttribOffset(GC3Duint index, GC3Denum pname);

    void hint(GC3Denum target, GC3Denum mode);
    GC3Dboolean isBuffer(WebGLBuffer*);
    bool isContextLost();
    GC3Dboolean isEnabled(GC3Denum cap);
    GC3Dboolean isFramebuffer(WebGLFramebuffer*);
    GC3Dboolean isProgram(WebGLProgram*);
    GC3Dboolean isRenderbuffer(WebGLRenderbuffer*);
    GC3Dboolean isShader(WebGLShader*);
    GC3Dboolean isTexture(WebGLTexture*);

    void lineWidth(GC3Dfloat);
    void linkProgram(WebGLProgram*);
    void pixelStorei(GC3Denum pname, GC3Dint param);
    void polygonOffset(GC3Dfloat factor, GC3Dfloat units);
    void readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, ArrayBufferView* pixels);
    void renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height);
    void sampleCoverage(GC3Dfloat value, GC3Dboolean invert);
    void scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);
    void shaderSource(WebGLShader*, const String&);
    void stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask);
    void stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask);
    void stencilMask(GC3Duint);
    void stencilMaskSeparate(GC3Denum face, GC3Duint mask);
    void stencilOp(GC3Denum fail, GC3Denum zfail, GC3Denum zpass);
    void stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zfail, GC3Denum zpass);

    void texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat,
        GC3Dsizei width, GC3Dsizei height, GC3Dint border,
        GC3Denum format, GC3Denum type, ArrayBufferView*, ExceptionState&);
    void texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat,
        GC3Denum format, GC3Denum type, ImageData*, ExceptionState&);
    void texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat,
        GC3Denum format, GC3Denum type, HTMLImageElement*, ExceptionState&);
    void texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat,
        GC3Denum format, GC3Denum type, HTMLCanvasElement*, ExceptionState&);
    void texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat,
        GC3Denum format, GC3Denum type, HTMLVideoElement*, ExceptionState&);

    void texParameterf(GC3Denum target, GC3Denum pname, GC3Dfloat param);
    void texParameteri(GC3Denum target, GC3Denum pname, GC3Dint param);

    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
        GC3Dsizei width, GC3Dsizei height,
        GC3Denum format, GC3Denum type, ArrayBufferView*, ExceptionState&);
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
        GC3Denum format, GC3Denum type, ImageData*, ExceptionState&);
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
        GC3Denum format, GC3Denum type, HTMLImageElement*, ExceptionState&);
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
        GC3Denum format, GC3Denum type, HTMLCanvasElement*, ExceptionState&);
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
        GC3Denum format, GC3Denum type, HTMLVideoElement*, ExceptionState&);

    void uniform1f(const WebGLUniformLocation*, GC3Dfloat x);
    void uniform1fv(const WebGLUniformLocation*, Float32Array* v);
    void uniform1fv(const WebGLUniformLocation*, GC3Dfloat* v, GC3Dsizei);
    void uniform1i(const WebGLUniformLocation*, GC3Dint x);
    void uniform1iv(const WebGLUniformLocation*, Int32Array* v);
    void uniform1iv(const WebGLUniformLocation*, GC3Dint* v, GC3Dsizei);
    void uniform2f(const WebGLUniformLocation*, GC3Dfloat x, GC3Dfloat y);
    void uniform2fv(const WebGLUniformLocation*, Float32Array* v);
    void uniform2fv(const WebGLUniformLocation*, GC3Dfloat* v, GC3Dsizei);
    void uniform2i(const WebGLUniformLocation*, GC3Dint x, GC3Dint y);
    void uniform2iv(const WebGLUniformLocation*, Int32Array* v);
    void uniform2iv(const WebGLUniformLocation*, GC3Dint* v, GC3Dsizei);
    void uniform3f(const WebGLUniformLocation*, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z);
    void uniform3fv(const WebGLUniformLocation*, Float32Array* v);
    void uniform3fv(const WebGLUniformLocation*, GC3Dfloat* v, GC3Dsizei);
    void uniform3i(const WebGLUniformLocation*, GC3Dint x, GC3Dint y, GC3Dint z);
    void uniform3iv(const WebGLUniformLocation*, Int32Array* v);
    void uniform3iv(const WebGLUniformLocation*, GC3Dint* v, GC3Dsizei);
    void uniform4f(const WebGLUniformLocation*, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w);
    void uniform4fv(const WebGLUniformLocation*, Float32Array* v);
    void uniform4fv(const WebGLUniformLocation*, GC3Dfloat* v, GC3Dsizei);
    void uniform4i(const WebGLUniformLocation*, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w);
    void uniform4iv(const WebGLUniformLocation*, Int32Array* v);
    void uniform4iv(const WebGLUniformLocation*, GC3Dint* v, GC3Dsizei);
    void uniformMatrix2fv(const WebGLUniformLocation*, GC3Dboolean transpose, Float32Array* value);
    void uniformMatrix2fv(const WebGLUniformLocation*, GC3Dboolean transpose, GC3Dfloat* value, GC3Dsizei);
    void uniformMatrix3fv(const WebGLUniformLocation*, GC3Dboolean transpose, Float32Array* value);
    void uniformMatrix3fv(const WebGLUniformLocation*, GC3Dboolean transpose, GC3Dfloat* value, GC3Dsizei);
    void uniformMatrix4fv(const WebGLUniformLocation*, GC3Dboolean transpose, Float32Array* value);
    void uniformMatrix4fv(const WebGLUniformLocation*, GC3Dboolean transpose, GC3Dfloat* value, GC3Dsizei);

    void useProgram(WebGLProgram*);
    void validateProgram(WebGLProgram*);

    void vertexAttrib1f(GC3Duint index, GC3Dfloat x);
    void vertexAttrib1fv(GC3Duint index, Float32Array* values);
    void vertexAttrib1fv(GC3Duint index, GC3Dfloat* values, GC3Dsizei);
    void vertexAttrib2f(GC3Duint index, GC3Dfloat x, GC3Dfloat y);
    void vertexAttrib2fv(GC3Duint index, Float32Array* values);
    void vertexAttrib2fv(GC3Duint index, GC3Dfloat* values, GC3Dsizei);
    void vertexAttrib3f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z);
    void vertexAttrib3fv(GC3Duint index, Float32Array* values);
    void vertexAttrib3fv(GC3Duint index, GC3Dfloat* values, GC3Dsizei);
    void vertexAttrib4f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w);
    void vertexAttrib4fv(GC3Duint index, Float32Array* values);
    void vertexAttrib4fv(GC3Duint index, GC3Dfloat* values, GC3Dsizei);
    void vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dboolean normalized,
        GC3Dsizei stride, long long offset);

    void vertexAttribDivisorANGLE(GC3Duint index, GC3Duint divisor);

    void viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);

    // WEBKIT_lose_context support
    enum LostContextMode {
        // Lost context occurred at the graphics system level.
        RealLostContext,

        // Lost context provoked by WEBKIT_lose_context.
        SyntheticLostContext,

        // A synthetic lost context that should attempt to recover automatically
        AutoRecoverSyntheticLostContext
    };
    void forceLostContext(LostContextMode);
    void forceRestoreContext();
    void loseContextImpl(LostContextMode);

    GraphicsContext3D* graphicsContext3D() const { return m_context.get(); }
    WebGLContextGroup* contextGroup() const { return m_contextGroup.get(); }
    virtual WebKit::WebLayer* platformLayer() const;

    void reshape(int width, int height);

    void markLayerComposited();
    virtual void paintRenderingResultsToCanvas();
    virtual PassRefPtr<ImageData> paintRenderingResultsToImageData();

    void removeSharedObject(WebGLSharedObject*);
    void removeContextObject(WebGLContextObject*);

    unsigned getMaxVertexAttribs() const { return m_maxVertexAttribs; }

    // ActiveDOMObject notifications
    virtual bool hasPendingActivity() const;
    virtual void stop();

  private:
    friend class WebGLDrawBuffers;
    friend class WebGLFramebuffer;
    friend class WebGLObject;
    friend class OESVertexArrayObject;
    friend class WebGLDebugShaders;
    friend class WebGLCompressedTextureATC;
    friend class WebGLCompressedTexturePVRTC;
    friend class WebGLCompressedTextureS3TC;
    friend class WebGLRenderingContextErrorMessageCallback;
    friend class WebGLVertexArrayObjectOES;

    WebGLRenderingContext(HTMLCanvasElement*, PassRefPtr<GraphicsContext3D>, GraphicsContext3D::Attributes, GraphicsContext3D::Attributes);
    void initializeNewContext();
    void setupFlags();

    void addSharedObject(WebGLSharedObject*);
    void addContextObject(WebGLContextObject*);
    void detachAndRemoveAllObjects();

    void destroyGraphicsContext3D();
    void markContextChanged();

    // Query if the GL implementation is NPOT strict.
    bool isGLES2NPOTStrict() { return m_isGLES2NPOTStrict; }
    // Query if depth_stencil buffer is supported.
    bool isDepthStencilSupported() { return m_isDepthStencilSupported; }

    // Helper to return the size in bytes of OpenGL data types
    // like GL_FLOAT, GL_INT, etc.
    unsigned int sizeInBytes(GC3Denum type);

    // Check if each enabled vertex attribute is bound to a buffer.
    bool validateRenderingState();

    bool validateWebGLObject(const char*, WebGLObject*);

    // Adds a compressed texture format.
    void addCompressedTextureFormat(GC3Denum);
    void removeAllCompressedTextureFormats();

    PassRefPtr<Image> videoFrameToImage(HTMLVideoElement*, BackingStoreCopy);

    WebGLRenderbuffer* ensureEmulatedStencilBuffer(GC3Denum target, WebGLRenderbuffer*);

    RefPtr<GraphicsContext3D> m_context;
    RefPtr<WebGLContextGroup> m_contextGroup;

    // Structure for rendering to a DrawingBuffer, instead of directly
    // to the back-buffer of m_context.
    RefPtr<DrawingBuffer> m_drawingBuffer;

    // Dispatches a context lost event once it is determined that one is needed.
    // This is used both for synthetic and real context losses. For real ones, it's
    // likely that there's no JavaScript on the stack, but that might be dependent
    // on how exactly the platform discovers that the context was lost. For better
    // portability we always defer the dispatch of the event.
    Timer<WebGLRenderingContext> m_dispatchContextLostEventTimer;
    bool m_restoreAllowed;
    Timer<WebGLRenderingContext> m_restoreTimer;

    bool m_needsUpdate;
    bool m_markedCanvasDirty;
    HashSet<WebGLContextObject*> m_contextObjects;

    // List of bound VBO's. Used to maintain info about sizes for ARRAY_BUFFER and stored values for ELEMENT_ARRAY_BUFFER
    RefPtr<WebGLBuffer> m_boundArrayBuffer;

    RefPtr<WebGLVertexArrayObjectOES> m_defaultVertexArrayObject;
    RefPtr<WebGLVertexArrayObjectOES> m_boundVertexArrayObject;
    void setBoundVertexArrayObject(PassRefPtr<WebGLVertexArrayObjectOES> arrayObject)
    {
        if (arrayObject)
            m_boundVertexArrayObject = arrayObject;
        else
            m_boundVertexArrayObject = m_defaultVertexArrayObject;
    }

    class VertexAttribValue {
    public:
        VertexAttribValue()
        {
            initValue();
        }

        void initValue()
        {
            value[0] = 0.0f;
            value[1] = 0.0f;
            value[2] = 0.0f;
            value[3] = 1.0f;
        }

        GC3Dfloat value[4];
    };
    Vector<VertexAttribValue> m_vertexAttribValue;
    unsigned m_maxVertexAttribs;
    RefPtr<WebGLBuffer> m_vertexAttrib0Buffer;
    long m_vertexAttrib0BufferSize;
    GC3Dfloat m_vertexAttrib0BufferValue[4];
    bool m_forceAttrib0BufferRefill;
    bool m_vertexAttrib0UsedBefore;

    RefPtr<WebGLProgram> m_currentProgram;
    RefPtr<WebGLFramebuffer> m_framebufferBinding;
    RefPtr<WebGLRenderbuffer> m_renderbufferBinding;
    class TextureUnitState {
    public:
        RefPtr<WebGLTexture> m_texture2DBinding;
        RefPtr<WebGLTexture> m_textureCubeMapBinding;
    };
    Vector<TextureUnitState> m_textureUnits;
    unsigned long m_activeTextureUnit;

    RefPtr<WebGLTexture> m_blackTexture2D;
    RefPtr<WebGLTexture> m_blackTextureCubeMap;

    Vector<GC3Denum> m_compressedTextureFormats;

    // Fixed-size cache of reusable image buffers for video texImage2D calls.
    class LRUImageBufferCache {
    public:
        LRUImageBufferCache(int capacity);
        // The pointer returned is owned by the image buffer map.
        ImageBuffer* imageBuffer(const IntSize& size);
    private:
        void bubbleToFront(int idx);
        OwnArrayPtr<OwnPtr<ImageBuffer> > m_buffers;
        int m_capacity;
    };
    LRUImageBufferCache m_videoCache;

    GC3Dint m_maxTextureSize;
    GC3Dint m_maxCubeMapTextureSize;
    GC3Dint m_maxRenderbufferSize;
    GC3Dint m_maxViewportDims[2];
    GC3Dint m_maxTextureLevel;
    GC3Dint m_maxCubeMapTextureLevel;

    GC3Dint m_maxDrawBuffers;
    GC3Dint m_maxColorAttachments;
    GC3Denum m_backDrawBuffer;
    bool m_drawBuffersWebGLRequirementsChecked;
    bool m_drawBuffersSupported;

    GC3Dint m_packAlignment;
    GC3Dint m_unpackAlignment;
    bool m_unpackFlipY;
    bool m_unpackPremultiplyAlpha;
    GC3Denum m_unpackColorspaceConversion;
    bool m_contextLost;
    LostContextMode m_contextLostMode;
    GraphicsContext3D::Attributes m_attributes;
    GraphicsContext3D::Attributes m_requestedAttributes;

    bool m_layerCleared;
    GC3Dfloat m_clearColor[4];
    bool m_scissorEnabled;
    GC3Dfloat m_clearDepth;
    GC3Dint m_clearStencil;
    GC3Dboolean m_colorMask[4];
    GC3Dboolean m_depthMask;

    bool m_stencilEnabled;
    GC3Duint m_stencilMask, m_stencilMaskBack;
    GC3Dint m_stencilFuncRef, m_stencilFuncRefBack; // Note that these are the user specified values, not the internal clamped value.
    GC3Duint m_stencilFuncMask, m_stencilFuncMaskBack;

    bool m_isGLES2NPOTStrict;
    bool m_isDepthStencilSupported;

    bool m_synthesizedErrorsToConsole;
    int m_numGLErrorsToConsoleAllowed;

    bool m_multisamplingAllowed;
    bool m_multisamplingObserverRegistered;

    GC3Duint m_onePlusMaxEnabledAttribIndex;
    unsigned long m_onePlusMaxNonDefaultTextureUnit;

    // Enabled extension objects.
    RefPtr<ANGLEInstancedArrays> m_angleInstancedArrays;
    RefPtr<EXTFragDepth> m_extFragDepth;
    RefPtr<EXTTextureFilterAnisotropic> m_extTextureFilterAnisotropic;
    RefPtr<OESTextureFloat> m_oesTextureFloat;
    RefPtr<OESTextureFloatLinear> m_oesTextureFloatLinear;
    RefPtr<OESTextureHalfFloat> m_oesTextureHalfFloat;
    RefPtr<OESTextureHalfFloatLinear> m_oesTextureHalfFloatLinear;
    RefPtr<OESStandardDerivatives> m_oesStandardDerivatives;
    RefPtr<OESVertexArrayObject> m_oesVertexArrayObject;
    RefPtr<OESElementIndexUint> m_oesElementIndexUint;
    RefPtr<WebGLLoseContext> m_webglLoseContext;
    RefPtr<WebGLDebugRendererInfo> m_webglDebugRendererInfo;
    RefPtr<WebGLDebugShaders> m_webglDebugShaders;
    RefPtr<WebGLDrawBuffers> m_webglDrawBuffers;
    RefPtr<WebGLCompressedTextureATC> m_webglCompressedTextureATC;
    RefPtr<WebGLCompressedTexturePVRTC> m_webglCompressedTexturePVRTC;
    RefPtr<WebGLCompressedTextureS3TC> m_webglCompressedTextureS3TC;
    RefPtr<WebGLDepthTexture> m_webglDepthTexture;

    enum ExtensionFlags {
        ApprovedExtension   = 0x00,
        DraftExtension      = 0x01,
        PrivilegedExtension = 0x02,
        PrefixedExtension   = 0x04,
    };

    class ExtensionTracker {
    public:
        ExtensionTracker(ExtensionFlags flags, const char** prefixes)
            : m_privileged(flags & PrivilegedExtension)
            , m_draft(flags & DraftExtension)
            , m_prefixed(flags & PrefixedExtension)
            , m_prefixes(prefixes)
        {
        }

        virtual ~ExtensionTracker()
        {
        }

        bool getPrefixed() const
        {
            return m_prefixed;
        }

        bool getPrivileged() const
        {
            return m_privileged;
        }

        bool getDraft() const
        {
            return m_draft;
        }

        bool matchesNameWithPrefixes(const String&) const;

        virtual PassRefPtr<WebGLExtension> getExtension(WebGLRenderingContext*) const = 0;
        virtual bool supported(WebGLRenderingContext*) const = 0;
        virtual const char* getExtensionName() const = 0;
        virtual void loseExtension() = 0;

    private:
        bool m_privileged;
        bool m_draft;
        bool m_prefixed;
        const char** m_prefixes;
    };

    template <typename T>
    class TypedExtensionTracker : public ExtensionTracker {
    public:
        TypedExtensionTracker(RefPtr<T>& extensionField, ExtensionFlags flags, const char** prefixes)
            : ExtensionTracker(flags, prefixes)
            , m_extensionField(extensionField)
        {
        }

        ~TypedExtensionTracker()
        {
            if (m_extensionField) {
                m_extensionField->lose(true);
                m_extensionField = 0;
            }
        }

        virtual PassRefPtr<WebGLExtension> getExtension(WebGLRenderingContext* context) const
        {
            if (!m_extensionField)
                m_extensionField = T::create(context);

            return m_extensionField;
        }

        virtual bool supported(WebGLRenderingContext* context) const
        {
            return T::supported(context);
        }

        virtual const char* getExtensionName() const
        {
            return T::getExtensionName();
        }

        virtual void loseExtension()
        {
            if (m_extensionField) {
                m_extensionField->lose(false);
                if (m_extensionField->isLost())
                    m_extensionField = 0;
            }
        }

    private:
        RefPtr<T>& m_extensionField;
    };

    Vector<ExtensionTracker*> m_extensions;

    template <typename T>
    void registerExtension(RefPtr<T>& extensionPtr, ExtensionFlags flags = ApprovedExtension, const char** prefixes = 0)
    {
        m_extensions.append(new TypedExtensionTracker<T>(extensionPtr, flags, prefixes));
    }

    // Errors raised by synthesizeGLError() while the context is lost.
    Vector<GC3Denum> lost_context_errors_;

    // Helpers for getParameter and others
    WebGLGetInfo getBooleanParameter(GC3Denum);
    WebGLGetInfo getBooleanArrayParameter(GC3Denum);
    WebGLGetInfo getFloatParameter(GC3Denum);
    WebGLGetInfo getIntParameter(GC3Denum);
    WebGLGetInfo getUnsignedIntParameter(GC3Denum);
    WebGLGetInfo getWebGLFloatArrayParameter(GC3Denum);
    WebGLGetInfo getWebGLIntArrayParameter(GC3Denum);

    // Clear the backbuffer if it was composited since the last operation.
    // clearMask is set to the bitfield of any clear that would happen anyway at this time
    // and the function returns true if that clear is now unnecessary.
    bool clearIfComposited(GC3Dbitfield clearMask = 0);

    // Helper to restore state that clearing the framebuffer may destroy.
    void restoreStateAfterClear();

    void texImage2DBase(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels, ExceptionState&);
    void texImage2DImpl(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Denum format, GC3Denum type, Image*, GraphicsContext3D::ImageHtmlDomSource, bool flipY, bool premultiplyAlpha, ExceptionState&);
    void texSubImage2DBase(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels, ExceptionState&);
    void texSubImage2DImpl(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, Image*, GraphicsContext3D::ImageHtmlDomSource, bool flipY, bool premultiplyAlpha, ExceptionState&);

    void handleTextureCompleteness(const char*, bool);
    void createFallbackBlackTextures1x1();

    // Helper function for copyTex{Sub}Image, check whether the internalformat
    // and the color buffer format of the current bound framebuffer combination
    // is valid.
    bool isTexInternalFormatColorBufferCombinationValid(GC3Denum texInternalFormat,
                                                        GC3Denum colorBufferFormat);

    // Helper function to get the bound framebuffer's color buffer format.
    GC3Denum getBoundFramebufferColorFormat();

    // Helper function to get the bound framebuffer's width.
    int getBoundFramebufferWidth();

    // Helper function to get the bound framebuffer's height.
    int getBoundFramebufferHeight();

    // Helper function to verify limits on the length of uniform and attribute locations.
    bool validateLocationLength(const char* functionName, const String&);

    // Helper function to check if size is non-negative.
    // Generate GL error and return false for negative inputs; otherwise, return true.
    bool validateSize(const char* functionName, GC3Dint x, GC3Dint y);

    // Helper function to check if all characters in the string belong to the
    // ASCII subset as defined in GLSL ES 1.0 spec section 3.1.
    bool validateString(const char* functionName, const String&);

    // Helper function to check target and texture bound to the target.
    // Generate GL errors and return 0 if target is invalid or texture bound is
    // null.  Otherwise, return the texture bound to the target.
    WebGLTexture* validateTextureBinding(const char* functionName, GC3Denum target, bool useSixEnumsForCubeMap);

    // Helper function to check input format/type for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncFormatAndType(const char* functionName, GC3Denum format, GC3Denum type, GC3Dint level);

    // Helper function to check input level for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if level is invalid.
    bool validateTexFuncLevel(const char* functionName, GC3Denum target, GC3Dint level);

    enum TexFuncValidationFunctionType {
        NotTexSubImage2D,
        TexSubImage2D,
    };

    enum TexFuncValidationSourceType {
        SourceArrayBufferView,
        SourceImageData,
        SourceHTMLImageElement,
        SourceHTMLCanvasElement,
        SourceHTMLVideoElement,
    };

    // Helper function for tex{Sub}Image2D to check if the input format/type/level/target/width/height/border/xoffset/yoffset are valid.
    // Otherwise, it would return quickly without doing other work.
    bool validateTexFunc(const char* functionName, TexFuncValidationFunctionType, TexFuncValidationSourceType, GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width,
        GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, GC3Dint xoffset, GC3Dint yoffset);

    // Helper function to check input width and height for functions {copy, compressed}Tex{Sub}Image.
    // Generates GL error and returns false if width or height is invalid.
    bool validateTexFuncDimensions(const char* functionName, TexFuncValidationFunctionType,
        GC3Denum target, GC3Dint level, GC3Dsizei width, GC3Dsizei height);

    // Helper function to check input parameters for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncParameters(const char* functionName,
                                   TexFuncValidationFunctionType,
                                   GC3Denum target, GC3Dint level,
                                   GC3Denum internalformat,
                                   GC3Dsizei width, GC3Dsizei height, GC3Dint border,
                                   GC3Denum format, GC3Denum type);

    enum NullDisposition {
        NullAllowed,
        NullNotAllowed
    };

    // Helper function to validate that the given ArrayBufferView
    // is of the correct type and contains enough data for the texImage call.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncData(const char* functionName, GC3Dint level,
                             GC3Dsizei width, GC3Dsizei height,
                             GC3Denum format, GC3Denum type,
                             ArrayBufferView* pixels,
                             NullDisposition);

    // Helper function to validate a given texture format is settable as in
    // you can supply data to texImage2D, or call texImage2D, copyTexImage2D and
    // copyTexSubImage2D.
    // Generates GL error and returns false if the format is not settable.
    bool validateSettableTexFormat(const char* functionName, GC3Denum format);

    // Helper function to validate compressed texture data is correct size
    // for the given format and dimensions.
    bool validateCompressedTexFuncData(const char* functionName,
                                       GC3Dsizei width, GC3Dsizei height,
                                       GC3Denum format, ArrayBufferView* pixels);

    // Helper function for validating compressed texture formats.
    bool validateCompressedTexFormat(GC3Denum format);

    // Helper function to validate compressed texture dimensions are valid for
    // the given format.
    bool validateCompressedTexDimensions(const char* functionName, TexFuncValidationFunctionType, GC3Denum target, GC3Dint level, GC3Dsizei width, GC3Dsizei height, GC3Denum format);

    // Helper function to validate compressed texture dimensions are valid for
    // the given format.
    bool validateCompressedTexSubDimensions(const char* functionName, GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
                                            GC3Dsizei width, GC3Dsizei height, GC3Denum format, WebGLTexture*);

    // Helper function to validate mode for draw{Arrays/Elements}.
    bool validateDrawMode(const char* functionName, GC3Denum);

    // Helper function to validate if front/back stencilMask and stencilFunc settings are the same.
    bool validateStencilSettings(const char* functionName);

    // Helper function to validate stencil or depth func.
    bool validateStencilOrDepthFunc(const char* functionName, GC3Denum);

    // Helper function for texParameterf and texParameteri.
    void texParameter(GC3Denum target, GC3Denum pname, GC3Dfloat parami, GC3Dint paramf, bool isFloat);

    // Helper function to print GL errors to console.
    void printGLErrorToConsole(const String&);

    // Helper function to print warnings to console. Currently
    // used only to warn about use of obsolete functions.
    void printWarningToConsole(const String&);

    // Helper function to validate input parameters for framebuffer functions.
    // Generate GL error if parameters are illegal.
    bool validateFramebufferFuncParameters(const char* functionName, GC3Denum target, GC3Denum attachment);

    // Helper function to validate blend equation mode.
    bool validateBlendEquation(const char* functionName, GC3Denum);

    // Helper function to validate blend func factors.
    bool validateBlendFuncFactors(const char* functionName, GC3Denum src, GC3Denum dst);

    // Helper function to validate a GL capability.
    bool validateCapability(const char* functionName, GC3Denum);

    // Helper function to validate input parameters for uniform functions.
    bool validateUniformParameters(const char* functionName, const WebGLUniformLocation*, Float32Array*, GC3Dsizei mod);
    bool validateUniformParameters(const char* functionName, const WebGLUniformLocation*, Int32Array*, GC3Dsizei mod);
    bool validateUniformParameters(const char* functionName, const WebGLUniformLocation*, void*, GC3Dsizei, GC3Dsizei mod);
    bool validateUniformMatrixParameters(const char* functionName, const WebGLUniformLocation*, GC3Dboolean transpose, Float32Array*, GC3Dsizei mod);
    bool validateUniformMatrixParameters(const char* functionName, const WebGLUniformLocation*, GC3Dboolean transpose, void*, GC3Dsizei, GC3Dsizei mod);

    // Helper function to validate parameters for bufferData.
    // Return the current bound buffer to target, or 0 if parameters are invalid.
    WebGLBuffer* validateBufferDataParameters(const char* functionName, GC3Denum target, GC3Denum usage);

    // Helper function for tex{Sub}Image2D to make sure image is ready and wouldn't taint Origin.
    bool validateHTMLImageElement(const char* functionName, HTMLImageElement*, ExceptionState&);

    // Helper function for tex{Sub}Image2D to make sure canvas is ready and wouldn't taint Origin.
    bool validateHTMLCanvasElement(const char* functionName, HTMLCanvasElement*, ExceptionState&);

    // Helper function for tex{Sub}Image2D to make sure video is ready wouldn't taint Origin.
    bool validateHTMLVideoElement(const char* functionName, HTMLVideoElement*, ExceptionState&);

    // Helper function to validate drawArrays(Instanced) calls
    bool validateDrawArrays(const char* functionName, GC3Denum mode, GC3Dint first, GC3Dsizei count);

    // Helper function to validate drawElements(Instanced) calls
    bool validateDrawElements(const char* functionName, GC3Denum mode, GC3Dsizei count, GC3Denum type, long long offset);

    // Helper function to validate draw*Instanced calls
    bool validateDrawInstanced(const char* functionName, GC3Dsizei primcount);

    // Helper functions for vertexAttribNf{v}.
    void vertexAttribfImpl(const char* functionName, GC3Duint index, GC3Dsizei expectedSize, GC3Dfloat, GC3Dfloat, GC3Dfloat, GC3Dfloat);
    void vertexAttribfvImpl(const char* functionName, GC3Duint index, Float32Array*, GC3Dsizei expectedSize);
    void vertexAttribfvImpl(const char* functionName, GC3Duint index, GC3Dfloat*, GC3Dsizei, GC3Dsizei expectedSize);

    // Helper function for delete* (deleteBuffer, deleteProgram, etc) functions.
    // Return false if caller should return without further processing.
    bool deleteObject(WebGLObject*);

    // Helper function for bind* (bindBuffer, bindTexture, etc) and useProgram.
    // If the object has already been deleted, set deleted to true upon return.
    // Return false if caller should return without further processing.
    bool checkObjectToBeBound(const char* functionName, WebGLObject*, bool& deleted);

    void dispatchContextLostEvent(Timer<WebGLRenderingContext>*);
    // Helper for restoration after context lost.
    void maybeRestoreContext(Timer<WebGLRenderingContext>*);

    // Determine if we are running privileged code in the browser, for example,
    // a Safari or Chrome extension.
    bool allowPrivilegedExtensions() const;

    enum ConsoleDisplayPreference {
        DisplayInConsole,
        DontDisplayInConsole
    };

    // Wrapper for GraphicsContext3D::synthesizeGLError that sends a message
    // to the JavaScript console.
    void synthesizeGLError(GC3Denum, const char* functionName, const char* description, ConsoleDisplayPreference = DisplayInConsole);
    void emitGLWarning(const char* function, const char* reason);

    String ensureNotNull(const String&) const;

    // Enable or disable stencil test based on user setting and
    // whether the current FBO has a stencil buffer.
    void applyStencilTest();

    // Helper for enabling or disabling a capability.
    void enableOrDisable(GC3Denum capability, bool enable);

    // Clamp the width and height to GL_MAX_VIEWPORT_DIMS.
    IntSize clampedCanvasSize();

    // First time called, if EXT_draw_buffers is supported, query the value; otherwise return 0.
    // Later, return the cached value.
    GC3Dint getMaxDrawBuffers();
    GC3Dint getMaxColorAttachments();

    void setBackDrawBuffer(GC3Denum);

    void restoreCurrentFramebuffer();
    void restoreCurrentTexture2D();

    virtual void multisamplingChanged(bool);

    void findNewMaxEnabledAttribIndex();
    void findNewMaxNonDefaultTextureUnit();

    friend class WebGLStateRestorer;
    friend class WebGLRenderingContextEvictionManager;

    static Vector<WebGLRenderingContext*>& activeContexts();
    static Vector<WebGLRenderingContext*>& forciblyEvictedContexts();

    static void activateContext(WebGLRenderingContext*);
    static void deactivateContext(WebGLRenderingContext*, bool addToInactiveList);
    static void willDestroyContext(WebGLRenderingContext*);
    static void forciblyLoseOldestContext(const String& reason);
    // Return the least recently used context's position in the active context vector.
    // If the vector is empty, return the maximum allowed active context number.
    static size_t oldestContextIndex();
    static IntSize oldestContextSize();
};

} // namespace WebCore

#endif
