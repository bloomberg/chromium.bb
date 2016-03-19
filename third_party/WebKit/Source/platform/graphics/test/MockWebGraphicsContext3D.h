/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MockWebGraphicsContext3D_h
#define MockWebGraphicsContext3D_h

#include "platform/PlatformExport.h"
#include "public/platform/WebGraphicsContext3D.h"

namespace blink {

// WebGraphicsContext3D base class for use in WebKit unit tests.
// All operations are no-ops (returning 0 if necessary).
class MockWebGraphicsContext3D : public WebGraphicsContext3D {
public:
    MockWebGraphicsContext3D()
        : m_nextTextureId(1)
        , m_contextLost(false)
    {
    }

    virtual void synthesizeGLError(WGC3Denum) { }

    virtual WebString getRequestableExtensionsCHROMIUM() { return WebString(); }

    virtual void blitFramebufferCHROMIUM(WGC3Dint srcX0, WGC3Dint srcY0, WGC3Dint srcX1, WGC3Dint srcY1, WGC3Dint dstX0, WGC3Dint dstY0, WGC3Dint dstX1, WGC3Dint dstY1, WGC3Dbitfield mask, WGC3Denum filter) { }

    virtual void drawElements(WGC3Denum mode, WGC3Dsizei count, WGC3Denum type, WGC3Dintptr offset) { }

    virtual bool getActiveAttrib(WebGLId program, WGC3Duint index, ActiveInfo&) { return false; }
    virtual bool getActiveUniform(WebGLId program, WGC3Duint index, ActiveInfo&) { return false; }
    virtual Attributes getContextAttributes() { return m_attrs; }
    virtual WGC3Denum getError() { return 0; }
    virtual WebString getProgramInfoLog(WebGLId program) { return WebString(); }
    virtual WebString getShaderInfoLog(WebGLId shader) { return WebString(); }
    virtual WebString getShaderSource(WebGLId shader) { return WebString(); }
    virtual WebString getString(WGC3Denum name) { return WebString(); }
    virtual WGC3Dsizeiptr getVertexAttribOffset(WGC3Duint index, WGC3Denum pname) { return 0; }

    virtual void shaderSource(WebGLId shader, const WGC3Dchar* string) { }

    virtual void texImage2D(WGC3Denum target, WGC3Dint level, WGC3Denum internalformat, WGC3Dsizei width, WGC3Dsizei height, WGC3Dint border, WGC3Denum format, WGC3Denum type, const void* pixels) { }

    virtual void texParameterf(WGC3Denum target, WGC3Denum pname, WGC3Dfloat param) { }
    virtual void texParameteri(WGC3Denum target, WGC3Denum pname, WGC3Dint param) { }

    virtual void texSubImage2D(WGC3Denum target, WGC3Dint level, WGC3Dint xoffset, WGC3Dint yoffset, WGC3Dsizei width, WGC3Dsizei height, WGC3Denum format, WGC3Denum type, const void* pixels) { }

    virtual void uniform1f(WGC3Dint location, WGC3Dfloat x) { }
    virtual void uniform1fv(WGC3Dint location, WGC3Dsizei count, const WGC3Dfloat* v) { }
    virtual void uniform1i(WGC3Dint location, WGC3Dint x) { }
    virtual void uniform1iv(WGC3Dint location, WGC3Dsizei count, const WGC3Dint* v) { }
    virtual void uniform2f(WGC3Dint location, WGC3Dfloat x, WGC3Dfloat y) { }
    virtual void uniform2fv(WGC3Dint location, WGC3Dsizei count, const WGC3Dfloat* v) { }
    virtual void uniform2i(WGC3Dint location, WGC3Dint x, WGC3Dint y) { }
    virtual void uniform2iv(WGC3Dint location, WGC3Dsizei count, const WGC3Dint* v) { }
    virtual void uniform3f(WGC3Dint location, WGC3Dfloat x, WGC3Dfloat y, WGC3Dfloat z) { }
    virtual void uniform3fv(WGC3Dint location, WGC3Dsizei count, const WGC3Dfloat* v) { }
    virtual void uniform3i(WGC3Dint location, WGC3Dint x, WGC3Dint y, WGC3Dint z) { }
    virtual void uniform3iv(WGC3Dint location, WGC3Dsizei count, const WGC3Dint* v) { }
    virtual void uniform4f(WGC3Dint location, WGC3Dfloat x, WGC3Dfloat y, WGC3Dfloat z, WGC3Dfloat w) { }
    virtual void uniform4fv(WGC3Dint location, WGC3Dsizei count, const WGC3Dfloat* v) { }
    virtual void uniform4i(WGC3Dint location, WGC3Dint x, WGC3Dint y, WGC3Dint z, WGC3Dint w) { }
    virtual void uniform4iv(WGC3Dint location, WGC3Dsizei count, const WGC3Dint* v) { }
    virtual void uniformMatrix2fv(WGC3Dint location, WGC3Dsizei count, WGC3Dboolean transpose, const WGC3Dfloat* value) { }
    virtual void uniformMatrix3fv(WGC3Dint location, WGC3Dsizei count, WGC3Dboolean transpose, const WGC3Dfloat* value) { }
    virtual void uniformMatrix4fv(WGC3Dint location, WGC3Dsizei count, WGC3Dboolean transpose, const WGC3Dfloat* value) { }

    virtual void useProgram(WebGLId program) { }
    virtual void validateProgram(WebGLId program) { }

    virtual void vertexAttrib1f(WGC3Duint index, WGC3Dfloat x) { }
    virtual void vertexAttrib1fv(WGC3Duint index, const WGC3Dfloat* values) { }
    virtual void vertexAttrib2f(WGC3Duint index, WGC3Dfloat x, WGC3Dfloat y) { }
    virtual void vertexAttrib2fv(WGC3Duint index, const WGC3Dfloat* values) { }
    virtual void vertexAttrib3f(WGC3Duint index, WGC3Dfloat x, WGC3Dfloat y, WGC3Dfloat z) { }
    virtual void vertexAttrib3fv(WGC3Duint index, const WGC3Dfloat* values) { }
    virtual void vertexAttrib4f(WGC3Duint index, WGC3Dfloat x, WGC3Dfloat y, WGC3Dfloat z, WGC3Dfloat w) { }
    virtual void vertexAttrib4fv(WGC3Duint index, const WGC3Dfloat* values) { }
    virtual void vertexAttribPointer(WGC3Duint index, WGC3Dint size, WGC3Denum type, WGC3Dboolean normalized, WGC3Dsizei stride, WGC3Dintptr offset) { }

    virtual void viewport(WGC3Dint x, WGC3Dint y, WGC3Dsizei width, WGC3Dsizei height) { }

    virtual void genBuffers(WGC3Dsizei count, WebGLId* ids)
    {
        for (int i = 0; i < count; ++i)
            ids[i] = 1;
    }
    virtual void genFramebuffers(WGC3Dsizei count, WebGLId* ids)
    {
        for (int i = 0; i < count; ++i)
            ids[i] = 1;
    }
    virtual void genRenderbuffers(WGC3Dsizei count, WebGLId* ids)
    {
        for (int i = 0; i < count; ++i)
            ids[i] = 1;
    }
    virtual void genTextures(WGC3Dsizei count, WebGLId* ids)
    {
        for (int i = 0; i < count; ++i)
            ids[i] = m_nextTextureId++;
    }

    virtual void deleteBuffers(WGC3Dsizei count, WebGLId* ids) { }
    virtual void deleteFramebuffers(WGC3Dsizei count, WebGLId* ids) { }
    virtual void deleteRenderbuffers(WGC3Dsizei count, WebGLId* ids) { }
    virtual void deleteTextures(WGC3Dsizei count, WebGLId* ids) { }

    virtual WebGLId createBuffer() { return 1; }
    virtual WebGLId createFramebuffer() { return 1; }
    virtual WebGLId createRenderbuffer() { return 1; }
    virtual WebGLId createTexture() { return m_nextTextureId++; }

    virtual void deleteBuffer(WebGLId) { }
    virtual void deleteFramebuffer(WebGLId) { }
    virtual void deleteRenderbuffer(WebGLId) { }
    virtual void deleteTexture(WebGLId) { }

    virtual WebGLId createProgram() { return 1; }
    virtual WebGLId createShader(WGC3Denum) { return 1; }

    virtual void deleteProgram(WebGLId) { }
    virtual void deleteShader(WebGLId) { }

    virtual void texStorage2DEXT(WGC3Denum target, WGC3Dint levels, WGC3Duint internalformat, WGC3Dint width, WGC3Dint height) { }

    virtual WebGLId createQueryEXT() { return 1; }
    virtual void deleteQueryEXT(WebGLId) { }

    virtual WebString getTranslatedShaderSourceANGLE(WebGLId) { return WebString(); }

    // Don't use this, make a MockGLES2Interface instead.
    virtual gpu::gles2::GLES2Interface* getGLES2Interface() { return nullptr; }

    void fakeContextLost() { m_contextLost = true; }
protected:
    unsigned m_nextTextureId;
    bool m_contextLost;
    Attributes m_attrs;
};

} // namespace blink

#endif // MockWebGraphicsContext3D_h
