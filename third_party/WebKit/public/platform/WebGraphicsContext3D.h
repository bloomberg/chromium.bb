/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebGraphicsContext3D_h
#define WebGraphicsContext3D_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebString.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

struct GrGLInterface;

namespace blink {

// WGC3D types match the corresponding GL types as defined in OpenGL ES 2.0
// header file gl2.h from khronos.org.
typedef char WGC3Dchar;
typedef unsigned WGC3Denum;
typedef unsigned char WGC3Dboolean;
typedef unsigned WGC3Dbitfield;
typedef signed char WGC3Dbyte;
typedef unsigned char WGC3Dubyte;
typedef short WGC3Dshort;
typedef unsigned short WGC3Dushort;
typedef int WGC3Dint;
typedef int WGC3Dsizei;
typedef unsigned WGC3Duint;
typedef float WGC3Dfloat;
typedef float WGC3Dclampf;
typedef signed long int WGC3Dintptr;
typedef signed long int WGC3Dsizeiptr;
typedef int64_t WGC3Dint64;
typedef uint64_t WGC3Duint64;

// Typedef for server-side objects like OpenGL textures and program objects.
typedef WGC3Duint WebGLId;

// This interface abstracts the operations performed by the
// GraphicsContext3D in order to implement WebGL. Nearly all of the
// methods exposed on this interface map directly to entry points in
// the OpenGL ES 2.0 API.
class WebGraphicsContext3D : public WebNonCopyable {
public:
    // Return value from getActiveUniform and getActiveAttrib.
    struct ActiveInfo {
        WebString name;
        WGC3Denum type;
        WGC3Dint size;
    };

    // Context creation attributes.
    struct Attributes {
        bool alpha = true;
        bool depth = true;
        bool stencil = true;
        bool antialias = true;
        bool premultipliedAlpha = true;
        bool canRecoverFromContextLoss = true;
        bool noExtensions = false;
        bool shareResources = true;
        bool preferDiscreteGPU = false;
        bool noAutomaticFlushes = false;
        bool failIfMajorPerformanceCaveat = false;
        bool webGL = false;
        unsigned webGLVersion = 0;
        // FIXME: ideally this would be a WebURL, but it is currently not
        // possible to pass a WebURL by value across the WebKit API boundary.
        // See https://bugs.webkit.org/show_bug.cgi?id=103793#c13 .
        WebString topDocumentURL;
    };

    struct WebGraphicsInfo {
        unsigned vendorId = 0;
        unsigned deviceId = 0;
        unsigned processCrashCount = 0;
        unsigned resetNotificationStrategy = 0;
        bool sandboxed = false;
        bool testFailContext = false;
        bool amdSwitchable = false;
        bool optimus = false;
        WebString vendorInfo;
        WebString rendererInfo;
        WebString driverVersion;
        WebString errorMessage;
    };

    class WebGraphicsContextLostCallback {
    public:
        virtual void onContextLost() = 0;

    protected:
        virtual ~WebGraphicsContextLostCallback() { }
    };

    class WebGraphicsErrorMessageCallback {
    public:
        virtual void onErrorMessage(const WebString&, WGC3Dint) = 0;

    protected:
        virtual ~WebGraphicsErrorMessageCallback() { }
    };

    // This destructor needs to be public so that using classes can destroy instances if initialization fails.
    virtual ~WebGraphicsContext3D() { }

    // GL_CHROMIUM_request_extension
    virtual WebString getRequestableExtensionsCHROMIUM() = 0;

    // The entry points below map directly to the OpenGL ES 2.0 API.
    // See: http://www.khronos.org/registry/gles/
    // and: http://www.khronos.org/opengles/sdk/docs/man/
    virtual bool getActiveAttrib(WebGLId program, WGC3Duint index, ActiveInfo&) = 0;
    virtual bool getActiveUniform(WebGLId program, WGC3Duint index, ActiveInfo&) = 0;
    virtual WebString getProgramInfoLog(WebGLId program) = 0;
    virtual WebString getShaderInfoLog(WebGLId shader) = 0;
    virtual WebString getShaderSource(WebGLId shader) = 0;
    virtual WebString getString(WGC3Denum name) = 0;

    virtual void shaderSource(WebGLId shader, const WGC3Dchar* string) = 0;

    virtual WebGLId createBuffer() = 0;
    virtual WebGLId createFramebuffer() = 0;
    virtual WebGLId createRenderbuffer() = 0;
    virtual WebGLId createTexture() = 0;

    virtual void deleteBuffer(WebGLId) = 0;
    virtual void deleteFramebuffer(WebGLId) = 0;
    virtual void deleteRenderbuffer(WebGLId) = 0;
    virtual void deleteTexture(WebGLId) = 0;

    virtual void setContextLostCallback(WebGraphicsContextLostCallback* callback) { }
    virtual void setErrorMessageCallback(WebGraphicsErrorMessageCallback* callback) { }

    virtual WebString getTranslatedShaderSourceANGLE(WebGLId shader) = 0;

    // GL_EXT_occlusion_query
    virtual WebGLId createQueryEXT() { return 0; }
    virtual void deleteQueryEXT(WebGLId query) { }

    // GL_CHROMIUM_subscribe_uniform
    virtual WebGLId createValuebufferCHROMIUM() { return 0; }
    virtual void deleteValuebufferCHROMIUM(WebGLId) { }

    // GL_EXT_debug_marker
    virtual void pushGroupMarkerEXT(const WGC3Dchar* marker) { }

    // GL_OES_vertex_array_object
    virtual WebGLId createVertexArrayOES() { return 0; }
    virtual void deleteVertexArrayOES(WebGLId array) { }

    // OpenGL ES 3.0 functions not represented by pre-existing extensions
    virtual void beginTransformFeedback(WGC3Denum primitiveMode) { }
    virtual void bindBufferBase(WGC3Denum target, WGC3Duint index, WebGLId buffer) { }
    virtual void bindBufferRange(WGC3Denum target, WGC3Duint index, WebGLId buffer, WGC3Dintptr offset, WGC3Dsizeiptr size) { }
    virtual void bindSampler(WGC3Duint unit, WebGLId sampler) { }
    virtual void bindTransformFeedback(WGC3Denum target, WebGLId transformfeedback) { }
    virtual void clearBufferfi(WGC3Denum buffer, WGC3Dint drawbuffer, WGC3Dfloat depth, WGC3Dint stencil) { }
    virtual void clearBufferfv(WGC3Denum buffer, WGC3Dint drawbuffer, const WGC3Dfloat *value) { }
    virtual void clearBufferiv(WGC3Denum buffer, WGC3Dint drawbuffer, const WGC3Dint *value) { }
    virtual void clearBufferuiv(WGC3Denum buffer, WGC3Dint drawbuffer, const WGC3Duint *value) { }
    virtual void compressedTexImage3D(WGC3Denum target, WGC3Dint level, WGC3Denum internalformat, WGC3Dsizei width, WGC3Dsizei height, WGC3Dsizei depth, WGC3Dint border, WGC3Dsizei imageSize, const void *data) { }
    virtual void compressedTexSubImage3D(WGC3Denum target, WGC3Dint level, WGC3Dint xoffset, WGC3Dint yoffset, WGC3Dint zoffset, WGC3Dsizei width, WGC3Dsizei height, WGC3Dsizei depth, WGC3Denum format, WGC3Dsizei imageSize, const void *data) { }
    virtual void copyBufferSubData(WGC3Denum readTarget, WGC3Denum writeTarget, WGC3Dintptr readOffset, WGC3Dintptr writeOffset, WGC3Dsizeiptr size) { }
    virtual void copyTexSubImage3D(WGC3Denum target, WGC3Dint level, WGC3Dint xoffset, WGC3Dint yoffset, WGC3Dint zoffset, WGC3Dint x, WGC3Dint y, WGC3Dsizei width, WGC3Dsizei height) { }
    virtual WebGLId createSampler() { return 0; }
    virtual WebGLId createTransformFeedback() { return 0; }
    virtual void deleteSampler(WebGLId sampler) { }
    virtual void deleteTransformFeedback(WebGLId transformfeedback) { }
    virtual void endTransformFeedback(void) { }
    virtual void getActiveUniformBlockName(WebGLId program, WGC3Duint uniformBlockIndex, WGC3Dsizei bufSize, WGC3Dsizei *length, WGC3Dchar *uniformBlockName) { }
    virtual void getActiveUniformBlockiv(WebGLId program, WGC3Duint uniformBlockIndex, WGC3Denum pname, WGC3Dint *params) { }
    virtual void getActiveUniformsiv(WebGLId program, WGC3Dsizei uniformCount, const WGC3Duint *uniformIndices, WGC3Denum pname, WGC3Dint *params) { }
    virtual void getBufferParameteri64v(WGC3Denum target, WGC3Denum pname, WGC3Dint64 *value) { }
    virtual WGC3Dint getFragDataLocation(WebGLId program, const WGC3Dchar *name) { return -1; }
    virtual void getInternalformativ(WGC3Denum target, WGC3Denum internalformat, WGC3Denum pname, WGC3Dsizei bufSize, WGC3Dint *params) { }
    virtual void getSamplerParameterfv(WebGLId sampler, WGC3Denum pname, WGC3Dfloat *params) { }
    virtual void getSamplerParameteriv(WebGLId sampler, WGC3Denum pname, WGC3Dint *params) { }
    virtual void getTransformFeedbackVarying(WebGLId program, WGC3Duint index, WGC3Dsizei bufSize, WGC3Dsizei *length, WGC3Dsizei *size, WGC3Denum *type, WGC3Dchar *name) { }
    virtual WGC3Duint getUniformBlockIndex(WebGLId program, const WGC3Dchar *uniformBlockName) { return 0xFFFFFFFFu; /* GL_INVALID_INDEX */ }
    virtual void getUniformIndices(WebGLId program, WGC3Dsizei uniformCount, const WGC3Dchar *const*uniformNames, WGC3Duint *uniformIndices) { }
    virtual void getUniformuiv(WebGLId program, WGC3Dint location, WGC3Duint *params) { }
    virtual void invalidateFramebuffer(WGC3Denum target, WGC3Dsizei numAttachments, const WGC3Denum *attachments) { }
    virtual void invalidateSubFramebuffer(WGC3Denum target, WGC3Dsizei numAttachments, const WGC3Denum *attachments, WGC3Dint x, WGC3Dint y, WGC3Dsizei width, WGC3Dsizei height) { }
    virtual WGC3Dboolean isSampler(WebGLId sampler) { return false; }
    virtual WGC3Dboolean isTransformFeedback(WebGLId transformfeedback) { return false; }
    virtual void* mapBufferRange(WGC3Denum target, WGC3Dintptr offset, WGC3Dsizeiptr length, WGC3Dbitfield access) { return 0; }
    virtual void pauseTransformFeedback(void) { }
    virtual void programParameteri(WebGLId program, WGC3Denum pname, WGC3Dint value) { }
    virtual void readBuffer(WGC3Denum src) { }
    virtual void resumeTransformFeedback(void) { }
    virtual void samplerParameterf(WebGLId sampler, WGC3Denum pname, WGC3Dfloat param) { }
    virtual void samplerParameterfv(WebGLId sampler, WGC3Denum pname, const WGC3Dfloat *param) { }
    virtual void samplerParameteri(WebGLId sampler, WGC3Denum pname, WGC3Dint param) { }
    virtual void samplerParameteriv(WebGLId sampler, WGC3Denum pname, const WGC3Dint *param) { }
    virtual void transformFeedbackVaryings(WebGLId program, WGC3Dsizei count, const WGC3Dchar *const*varyings, WGC3Denum bufferMode) { }
    virtual WGC3Dboolean unmapBuffer(WGC3Denum target) { return false; }

    // Prefer getting a GLES2Interface off WebGraphicsContext3DProvider if possible, and avoid using WebGraphicsContext3D at all.
    virtual gpu::gles2::GLES2Interface* getGLES2Interface() = 0;
};

} // namespace blink

#endif
