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

    virtual void setContextLostCallback(WebGraphicsContextLostCallback* callback) { }
    virtual void setErrorMessageCallback(WebGraphicsErrorMessageCallback* callback) { }

    virtual WebString getTranslatedShaderSourceANGLE(WebGLId shader) = 0;

    // GL_EXT_debug_marker
    virtual void pushGroupMarkerEXT(const WGC3Dchar* marker) { }

    // Prefer getting a GLES2Interface off WebGraphicsContext3DProvider if possible, and avoid using WebGraphicsContext3D at all.
    virtual gpu::gles2::GLES2Interface* getGLES2Interface() = 0;
};

} // namespace blink

#endif
