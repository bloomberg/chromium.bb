// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/canvas/WebGL2RenderingContextBase.h"

#include "bindings/core/v8/WebGLAny.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/html/canvas/WebGLActiveInfo.h"
#include "core/html/canvas/WebGLBuffer.h"
#include "core/html/canvas/WebGLFenceSync.h"
#include "core/html/canvas/WebGLFramebuffer.h"
#include "core/html/canvas/WebGLProgram.h"
#include "core/html/canvas/WebGLQuery.h"
#include "core/html/canvas/WebGLSampler.h"
#include "core/html/canvas/WebGLSync.h"
#include "core/html/canvas/WebGLTexture.h"
#include "core/html/canvas/WebGLTransformFeedback.h"
#include "core/html/canvas/WebGLUniformLocation.h"
#include "core/html/canvas/WebGLVertexArrayObjectOES.h"

#include "platform/NotImplemented.h"

namespace blink {

namespace {

const GLuint webGLTimeoutIgnored = 0xFFFFFFFF;

WGC3Dsync syncObjectOrZero(const WebGLSync* object)
{
    return object ? object->object() : 0;
}

}

WebGL2RenderingContextBase::WebGL2RenderingContextBase(HTMLCanvasElement* passedCanvas, PassOwnPtr<blink::WebGraphicsContext3D> context, const WebGLContextAttributes& requestedAttributes)
    : WebGLRenderingContextBase(passedCanvas, context, requestedAttributes)
{

}

WebGL2RenderingContextBase::~WebGL2RenderingContextBase()
{

}

void WebGL2RenderingContextBase::copyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
    if (isContextLost())
        return;

    webContext()->copyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}

void WebGL2RenderingContextBase::getBufferSubData(GLenum target, GLintptr offset, DOMArrayBuffer* returnedData)
{
    if (isContextLost())
        return;

    notImplemented();
}

void WebGL2RenderingContextBase::blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    if (isContextLost())
        return;

    webContext()->blitFramebufferCHROMIUM(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void WebGL2RenderingContextBase::framebufferTextureLayer(GLenum target, GLenum attachment, const WebGLTexture* texture, GLint level, GLint layer)
{
    if (isContextLost())
        return;

    if (texture && !texture->validate(contextGroup(), this)) {
        synthesizeGLError(GL_INVALID_VALUE, "framebufferTextureLayer", "no texture or texture not from this context");
        return;
    }

    webContext()->framebufferTextureLayer(target, attachment, objectOrZero(texture), level, layer);
}

ScriptValue WebGL2RenderingContextBase::getInternalformatParameter(ScriptState* scriptState, GLenum target, GLenum internalformat, GLenum pname)
{
    if (isContextLost())
        return ScriptValue::createNull(scriptState);

    notImplemented();
    return ScriptValue::createNull(scriptState);
}

void WebGL2RenderingContextBase::invalidateFramebuffer(GLenum target, Vector<GLenum>& attachments)
{
    if (isContextLost())
        return;

    webContext()->invalidateFramebuffer(target, attachments.size(), attachments.data());
}

void WebGL2RenderingContextBase::invalidateSubFramebuffer(GLenum target, Vector<GLenum>& attachments, GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (isContextLost())
        return;

    webContext()->invalidateSubFramebuffer(target, attachments.size(), attachments.data(), x, y, width, height);
}

void WebGL2RenderingContextBase::readBuffer(GLenum mode)
{
    if (isContextLost())
        return;

    webContext()->readBuffer(mode);
}

void WebGL2RenderingContextBase::renderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
    if (isContextLost())
        return;

    webContext()->renderbufferStorageMultisampleEXT(target, samples, internalformat, width, height);
}

/* Texture objects */
void WebGL2RenderingContextBase::texStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
    if (isContextLost())
        return;

    webContext()->texStorage2DEXT(target, levels, internalformat, width, height);
}

void WebGL2RenderingContextBase::texStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    if (isContextLost())
        return;

    webContext()->texStorage3D(target, levels, internalformat, width, height, depth);
}

void WebGL2RenderingContextBase::texImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, DOMArrayBufferView* pixels)
{
    if (isContextLost())
        return;

    notImplemented();
}

void WebGL2RenderingContextBase::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, DOMArrayBufferView* pixels)
{
    if (isContextLost() || !pixels)
        return;

    // FIXME: Ensure pixels is large enough to contain the desired texture dimensions.

    void* data = pixels->baseAddress();
    Vector<uint8_t> tempData;
    bool changeUnpackAlignment = false;
    if (data && (m_unpackFlipY || m_unpackPremultiplyAlpha)) {
        if (!WebGLImageConversion::extractTextureData(width, height, format, type,
            m_unpackAlignment,
            m_unpackFlipY, m_unpackPremultiplyAlpha,
            data,
            tempData))
            return;
        data = tempData.data();
        changeUnpackAlignment = true;
    }
    if (changeUnpackAlignment)
        webContext()->pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    webContext()->texSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data);
    if (changeUnpackAlignment)
        webContext()->pixelStorei(GL_UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGL2RenderingContextBase::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLenum format, GLenum type, ImageData* pixels)
{
    if (isContextLost() || !pixels)
        return;

    Vector<uint8_t> data;
    bool needConversion = true;
    // The data from ImageData is always of format RGBA8.
    // No conversion is needed if destination format is RGBA and type is USIGNED_BYTE and no Flip or Premultiply operation is required.
    if (format == GL_RGBA && type == GL_UNSIGNED_BYTE && !m_unpackFlipY && !m_unpackPremultiplyAlpha) {
        needConversion = false;
    } else {
        if (!WebGLImageConversion::extractImageData(pixels->data()->data(), pixels->size(), format, type, m_unpackFlipY, m_unpackPremultiplyAlpha, data)) {
            synthesizeGLError(GL_INVALID_VALUE, "texSubImage3D", "bad image data");
            return;
        }
    }
    if (m_unpackAlignment != 1)
        webContext()->pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    webContext()->texSubImage3D(target, level, xoffset, yoffset, zoffset, pixels->width(), pixels->height(), 1, format, type, needConversion ? data.data() : pixels->data()->data());
    if (m_unpackAlignment != 1)
        webContext()->pixelStorei(GL_UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGL2RenderingContextBase::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLenum format, GLenum type, HTMLImageElement* image)
{
    if (isContextLost() || !image)
        return;

    notImplemented();
}

void WebGL2RenderingContextBase::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLenum format, GLenum type, HTMLCanvasElement* canvas)
{
    if (isContextLost())
        return;

    notImplemented();
}

void WebGL2RenderingContextBase::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLenum format, GLenum type, HTMLVideoElement* video)
{
    if (isContextLost())
        return;

    notImplemented();
}

void WebGL2RenderingContextBase::copyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (isContextLost())
        return;

    notImplemented();
}

void WebGL2RenderingContextBase::compressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, DOMArrayBufferView* data)
{
    if (isContextLost())
        return;

    notImplemented();
}

void WebGL2RenderingContextBase::compressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, DOMArrayBufferView* data)
{
    if (isContextLost())
        return;

    notImplemented();
}

GLint WebGL2RenderingContextBase::getFragDataLocation(WebGLProgram* program, const String& name)
{
    if (isContextLost() || !validateWebGLObject("getFragDataLocation", program))
        return -1;

    return webContext()->getFragDataLocation(objectOrZero(program), name.utf8().data());
}

void WebGL2RenderingContextBase::uniform1ui(const WebGLUniformLocation* location, GLuint v0)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform1ui", "location not for current program");
        return;
    }

    webContext()->uniform1ui(location->location(), v0);
}

void WebGL2RenderingContextBase::uniform2ui(const WebGLUniformLocation* location, GLuint v0, GLuint v1)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform2ui", "location not for current program");
        return;
    }

    webContext()->uniform2ui(location->location(), v0, v1);
}

void WebGL2RenderingContextBase::uniform3ui(const WebGLUniformLocation* location, GLuint v0, GLuint v1, GLuint v2)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform3ui", "location not for current program");
        return;
    }

    webContext()->uniform3ui(location->location(), v0, v1, v2);
}

void WebGL2RenderingContextBase::uniform4ui(const WebGLUniformLocation* location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform4ui", "location not for current program");
        return;
    }

    webContext()->uniform4ui(location->location(), v0, v1, v2, v3);
}

void WebGL2RenderingContextBase::uniform1uiv(const WebGLUniformLocation* location, Vector<GLuint>& value)
{
    if (isContextLost() || validateUniformParameters("uniform1uiv", location, value.data(), value.size(), 1))
        return;

    webContext()->uniform1uiv(location->location(), value.size(), value.data());
}

void WebGL2RenderingContextBase::uniform2uiv(const WebGLUniformLocation* location, Vector<GLuint>& value)
{
    if (isContextLost() || !validateUniformParameters("uniform2uiv", location, value.data(), value.size(), 2))
        return;

    webContext()->uniform2uiv(location->location(), value.size() / 2, value.data());
}

void WebGL2RenderingContextBase::uniform3uiv(const WebGLUniformLocation* location, Vector<GLuint>& value)
{
    if (isContextLost() || !validateUniformParameters("uniform3uiv", location, value.data(), value.size(), 3))
        return;

    webContext()->uniform3uiv(location->location(), value.size() / 3, value.data());
}

void WebGL2RenderingContextBase::uniform4uiv(const WebGLUniformLocation* location, Vector<GLuint>& value)
{
    if (isContextLost() || !validateUniformParameters("uniform4uiv", location, value.data(), value.size(), 4))
        return;

    webContext()->uniform4uiv(location->location(), value.size() / 4, value.data());
}

void WebGL2RenderingContextBase::uniformMatrix2x3fv(const WebGLUniformLocation* location, GLboolean transpose, DOMFloat32Array* value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix2x3fv", location, transpose, value, 6))
        return;
    webContext()->uniformMatrix2x3fv(location->location(), value->length() / 6, transpose, value->data());
}

void WebGL2RenderingContextBase::uniformMatrix2x3fv(const WebGLUniformLocation* location, GLboolean transpose, Vector<GLfloat>& value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix2x3fv", location, transpose, value.data(), value.size(), 6))
        return;
    webContext()->uniformMatrix2x3fv(location->location(), value.size() / 6, transpose, value.data());
}

void WebGL2RenderingContextBase::uniformMatrix3x2fv(const WebGLUniformLocation* location, GLboolean transpose, DOMFloat32Array* value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix3x2fv", location, transpose, value, 6))
        return;
    webContext()->uniformMatrix3x2fv(location->location(), value->length() / 6, transpose, value->data());
}

void WebGL2RenderingContextBase::uniformMatrix3x2fv(const WebGLUniformLocation* location, GLboolean transpose, Vector<GLfloat>& value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix3x2fv", location, transpose, value.data(), value.size(), 6))
        return;
    webContext()->uniformMatrix3x2fv(location->location(), value.size() / 6, transpose, value.data());
}

void WebGL2RenderingContextBase::uniformMatrix2x4fv(const WebGLUniformLocation* location, GLboolean transpose, DOMFloat32Array* value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix2x4fv", location, transpose, value, 8))
        return;
    webContext()->uniformMatrix2x4fv(location->location(), value->length() / 8, transpose, value->data());
}

void WebGL2RenderingContextBase::uniformMatrix2x4fv(const WebGLUniformLocation* location, GLboolean transpose, Vector<GLfloat>& value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix2x4fv", location, transpose, value.data(), value.size(), 8))
        return;
    webContext()->uniformMatrix2x4fv(location->location(), value.size() / 8, transpose, value.data());
}

void WebGL2RenderingContextBase::uniformMatrix4x2fv(const WebGLUniformLocation* location, GLboolean transpose, DOMFloat32Array* value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix4x2fv", location, transpose, value, 8))
        return;
    webContext()->uniformMatrix4x2fv(location->location(), value->length() / 8, transpose, value->data());
}

void WebGL2RenderingContextBase::uniformMatrix4x2fv(const WebGLUniformLocation* location, GLboolean transpose, Vector<GLfloat>& value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix4x2fv", location, transpose, value.data(), value.size(), 8))
        return;
    webContext()->uniformMatrix4x2fv(location->location(), value.size() / 8, transpose, value.data());
}

void WebGL2RenderingContextBase::uniformMatrix3x4fv(const WebGLUniformLocation* location, GLboolean transpose, DOMFloat32Array* value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix3x4fv", location, transpose, value, 12))
        return;
    webContext()->uniformMatrix3x4fv(location->location(), value->length() / 12, transpose, value->data());
}

void WebGL2RenderingContextBase::uniformMatrix3x4fv(const WebGLUniformLocation* location, GLboolean transpose, Vector<GLfloat>& value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix3x4fv", location, transpose, value.data(), value.size(), 12))
        return;
    webContext()->uniformMatrix3x4fv(location->location(), value.size() / 12, transpose, value.data());
}

void WebGL2RenderingContextBase::uniformMatrix4x3fv(const WebGLUniformLocation* location, GLboolean transpose, DOMFloat32Array* value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix4x3fv", location, transpose, value, 12))
        return;
    webContext()->uniformMatrix4x3fv(location->location(), value->length() / 12, transpose, value->data());
}

void WebGL2RenderingContextBase::uniformMatrix4x3fv(const WebGLUniformLocation* location, GLboolean transpose, Vector<GLfloat>& value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix4x3fv", location, transpose, value.data(), value.size(), 12))
        return;
    webContext()->uniformMatrix4x3fv(location->location(), value.size() / 12, transpose, value.data());
}

void WebGL2RenderingContextBase::vertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w)
{
    if (isContextLost())
        return;

    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribI4i", "index out of range");
        return;
    }

    webContext()->vertexAttribI4i(index, x, y, z, w);
    // FIXME: Pretty sure this won't do what we want it to do. Same with the next 3 functions.
    VertexAttribValue& attribValue = m_vertexAttribValue[index];
    attribValue.value[0] = x;
    attribValue.value[1] = y;
    attribValue.value[2] = z;
    attribValue.value[3] = w;
}

void WebGL2RenderingContextBase::vertexAttribI4iv(GLuint index, const Vector<GLint>& value)
{
    if (isContextLost())
        return;

    if (!value.data()) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribI4iv", "no array");
        return;
    }
    if (value.size() < 4) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribI4iv", "invalid size");
        return;
    }
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribI4iv", "index out of range");
        return;
    }

    webContext()->vertexAttribI4iv(index, value.data());
    VertexAttribValue& attribValue = m_vertexAttribValue[index];
    attribValue.value[0] = value[0];
    attribValue.value[1] = value[1];
    attribValue.value[2] = value[2];
    attribValue.value[3] = value[3];
}

void WebGL2RenderingContextBase::vertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    if (isContextLost())
        return;

    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribI4ui", "index out of range");
        return;
    }

    webContext()->vertexAttribI4ui(index, x, y, z, w);
    VertexAttribValue& attribValue = m_vertexAttribValue[index];
    attribValue.value[0] = x;
    attribValue.value[1] = y;
    attribValue.value[2] = z;
    attribValue.value[3] = w;
}

void WebGL2RenderingContextBase::vertexAttribI4uiv(GLuint index, const Vector<GLuint>& value)
{
    if (isContextLost())
        return;

    if (!value.data()) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribI4uiv", "no array");
        return;
    }
    if (value.size() < 4) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribI4uiv", "invalid size");
        return;
    }
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribI4uiv", "index out of range");
        return;
    }

    webContext()->vertexAttribI4uiv(index, value.data());
    VertexAttribValue& attribValue = m_vertexAttribValue[index];
    attribValue.value[0] = value[0];
    attribValue.value[1] = value[1];
    attribValue.value[2] = value[2];
    attribValue.value[3] = value[3];
}

void WebGL2RenderingContextBase::vertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset)
{
    if (isContextLost())
        return;
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribIPointer", "index out of range");
        return;
    }
    if (size < 1 || size > 4 || stride < 0 || stride > 255) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribIPointer", "bad size or stride");
        return;
    }
    if (!validateValueFitNonNegInt32("vertexAttribIPointer", "offset", offset))
        return;
    if (!m_boundArrayBuffer) {
        synthesizeGLError(GL_INVALID_OPERATION, "vertexAttribIPointer", "no bound ARRAY_BUFFER");
        return;
    }
    unsigned typeSize = sizeInBytes(type);
    ASSERT((typeSize & (typeSize - 1)) == 0); // Ensure that the value is POT.
    if ((stride & (typeSize - 1)) || (static_cast<GLintptr>(offset) & (typeSize - 1))) {
        synthesizeGLError(GL_INVALID_OPERATION, "vertexAttribIPointer", "stride or offset not valid for type");
        return;
    }
    GLsizei bytesPerElement = size * typeSize;

    m_boundVertexArrayObject->setVertexAttribState(index, bytesPerElement, size, type, false, stride, static_cast<GLintptr>(offset), m_boundArrayBuffer);
    webContext()->vertexAttribIPointer(index, size, type, stride, static_cast<GLintptr>(offset));
}

/* Writing to the drawing buffer */
void WebGL2RenderingContextBase::vertexAttribDivisor(GLuint index, GLuint divisor)
{
    if (isContextLost())
        return;

    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribDivisor", "index out of range");
        return;
    }

    m_boundVertexArrayObject->setVertexAttribDivisor(index, divisor);
    webContext()->vertexAttribDivisorANGLE(index, divisor);
}

void WebGL2RenderingContextBase::drawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instanceCount)
{
    if (!validateDrawArrays("drawArraysInstanced", mode, first, count))
        return;

    if (!validateDrawInstanced("drawArraysInstanced", instanceCount))
        return;

    clearIfComposited();

    handleTextureCompleteness("drawArraysInstanced", true);
    webContext()->drawArraysInstancedANGLE(mode, first, count, instanceCount);
    handleTextureCompleteness("drawArraysInstanced", false);
    markContextChanged(CanvasChanged);
}

void WebGL2RenderingContextBase::drawElementsInstanced(GLenum mode, GLsizei count, GLenum type, GLintptr offset, GLsizei instanceCount)
{
    if (!validateDrawElements("drawElementsInstanced", mode, count, type, offset))
        return;

    if (!validateDrawInstanced("drawElementsInstanced", instanceCount))
        return;

    clearIfComposited();

    handleTextureCompleteness("drawElementsInstanced", true);
    webContext()->drawElementsInstancedANGLE(mode, count, type, static_cast<GLintptr>(offset), instanceCount);
    handleTextureCompleteness("drawElementsInstanced", false);
    markContextChanged(CanvasChanged);
}

void WebGL2RenderingContextBase::drawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, GLintptr offset)
{
    if (isContextLost())
        return;

    notImplemented();
}

void WebGL2RenderingContextBase::drawBuffers(const Vector<GLenum>& buffers)
{
    if (isContextLost())
        return;

    GLsizei n = buffers.size();
    const GLenum* bufs = buffers.data();
    if (!m_framebufferBinding) {
        if (n != 1) {
            synthesizeGLError(GL_INVALID_VALUE, "drawBuffers", "more than one buffer");
            return;
        }
        if (bufs[0] != GL_BACK && bufs[0] != GL_NONE) {
            synthesizeGLError(GL_INVALID_OPERATION, "drawBuffers", "BACK or NONE");
            return;
        }
        // Because the backbuffer is simulated on all current WebKit ports, we need to change BACK to COLOR_ATTACHMENT0.
        GLenum value = (bufs[0] == GL_BACK) ? GL_COLOR_ATTACHMENT0 : GL_NONE;
        webContext()->drawBuffersEXT(1, &value);
        setBackDrawBuffer(bufs[0]);
    } else {
        if (n > maxDrawBuffers()) {
            synthesizeGLError(GL_INVALID_VALUE, "drawBuffers", "more than max draw buffers");
            return;
        }
        for (GLsizei i = 0; i < n; ++i) {
            if (bufs[i] != GL_NONE && bufs[i] != static_cast<GLenum>(GL_COLOR_ATTACHMENT0_EXT + i)) {
                synthesizeGLError(GL_INVALID_OPERATION, "drawBuffers", "COLOR_ATTACHMENTi_EXT or NONE");
                return;
            }
        }
        m_framebufferBinding->drawBuffers(buffers);
    }
}

bool WebGL2RenderingContextBase::validateClearBuffer(const char* functionName, GLenum buffer, GLsizei size)
{
    switch (buffer) {
    case GL_COLOR:
    case GL_FRONT:
    case GL_BACK:
    case GL_FRONT_AND_BACK: {
        if (size < 4) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "invalid array size");
            return false;
        }
        break;
    }
    case GL_DEPTH:
    case GL_STENCIL: {
        if (size < 1) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "invalid array size");
            return false;
        }
        break;
    }
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid buffer");
        return false;
    }
    return true;
}

void WebGL2RenderingContextBase::clearBufferiv(GLenum buffer, GLint drawbuffer, DOMInt32Array* value)
{
    if (isContextLost() || !validateClearBuffer("clearBufferiv", buffer, value->length()))
        return;

    webContext()->clearBufferiv(buffer, drawbuffer, value->data());
}

void WebGL2RenderingContextBase::clearBufferiv(GLenum buffer, GLint drawbuffer, const Vector<GLint>& value)
{
    if (isContextLost() || !validateClearBuffer("clearBufferiv", buffer, value.size()))
        return;

    webContext()->clearBufferiv(buffer, drawbuffer, value.data());
}

void WebGL2RenderingContextBase::clearBufferuiv(GLenum buffer, GLint drawbuffer, DOMUint32Array* value)
{
    if (isContextLost() || !validateClearBuffer("clearBufferuiv", buffer, value->length()))
        return;

    webContext()->clearBufferuiv(buffer, drawbuffer, value->data());
}

void WebGL2RenderingContextBase::clearBufferuiv(GLenum buffer, GLint drawbuffer, const Vector<GLuint>& value)
{
    if (isContextLost() || !validateClearBuffer("clearBufferuiv", buffer, value.size()))
        return;

    webContext()->clearBufferuiv(buffer, drawbuffer, value.data());
}

void WebGL2RenderingContextBase::clearBufferfv(GLenum buffer, GLint drawbuffer, DOMFloat32Array* value)
{
    if (isContextLost() || !validateClearBuffer("clearBufferfv", buffer, value->length()))
        return;

    webContext()->clearBufferfv(buffer, drawbuffer, value->data());
}

void WebGL2RenderingContextBase::clearBufferfv(GLenum buffer, GLint drawbuffer, const Vector<GLfloat>& value)
{
    if (isContextLost() || !validateClearBuffer("clearBufferfv", buffer, value.size()))
        return;

    webContext()->clearBufferfv(buffer, drawbuffer, value.data());
}

void WebGL2RenderingContextBase::clearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    if (isContextLost())
        return;

    webContext()->clearBufferfi(buffer, drawbuffer, depth, stencil);
}

PassRefPtrWillBeRawPtr<WebGLQuery> WebGL2RenderingContextBase::createQuery()
{
    if (isContextLost())
        return nullptr;
    RefPtrWillBeRawPtr<WebGLQuery> o = WebGLQuery::create(this);
    addSharedObject(o.get());
    return o;
}

void WebGL2RenderingContextBase::deleteQuery(WebGLQuery* query)
{
    deleteObject(query);
}

GLboolean WebGL2RenderingContextBase::isQuery(WebGLQuery* query)
{
    if (isContextLost() || !query)
        return 0;

    return webContext()->isQueryEXT(query->object());
}

void WebGL2RenderingContextBase::beginQuery(GLenum target, WebGLQuery* query)
{
    if (isContextLost() || !validateWebGLObject("beginQuery", query))
        return;

    webContext()->beginQueryEXT(target, query->object());
}

void WebGL2RenderingContextBase::endQuery(GLenum target)
{
    if (isContextLost())
        return;

    webContext()->endQueryEXT(target);
}

PassRefPtrWillBeRawPtr<WebGLQuery> WebGL2RenderingContextBase::getQuery(GLenum target, GLenum pname)
{
    if (isContextLost())
        return nullptr;

    notImplemented();
    return nullptr;
}

ScriptValue WebGL2RenderingContextBase::getQueryParameter(ScriptState* scriptState, WebGLQuery* query, GLenum pname)
{
    if (isContextLost() || !validateWebGLObject("getQueryParameter", query))
        return ScriptValue::createNull(scriptState);

    notImplemented();
    return ScriptValue::createNull(scriptState);
}

PassRefPtrWillBeRawPtr<WebGLSampler> WebGL2RenderingContextBase::createSampler()
{
    if (isContextLost())
        return nullptr;
    RefPtrWillBeRawPtr<WebGLSampler> o = WebGLSampler::create(this);
    addSharedObject(o.get());
    return o;
}

void WebGL2RenderingContextBase::deleteSampler(WebGLSampler* sampler)
{
    deleteObject(sampler);
}

GLboolean WebGL2RenderingContextBase::isSampler(WebGLSampler* sampler)
{
    if (isContextLost() || !sampler)
        return 0;

    return webContext()->isSampler(sampler->object());
}

void WebGL2RenderingContextBase::bindSampler(GLuint unit, WebGLSampler* sampler)
{
    if (isContextLost() || !validateWebGLObject("bindSampler", sampler))
        return;

    webContext()->bindSampler(unit, objectOrZero(sampler));
}

void WebGL2RenderingContextBase::samplerParameteri(WebGLSampler* sampler, GLenum pname, GLint param)
{
    if (isContextLost() || !validateWebGLObject("samplerParameteri", sampler))
        return;

    webContext()->samplerParameteri(objectOrZero(sampler), pname, param);
}

void WebGL2RenderingContextBase::samplerParameterf(WebGLSampler* sampler, GLenum pname, GLfloat param)
{
    if (isContextLost() || !validateWebGLObject("samplerParameterf", sampler))
        return;

    webContext()->samplerParameterf(objectOrZero(sampler), pname, param);
}

ScriptValue WebGL2RenderingContextBase::getSamplerParameter(ScriptState* scriptState, WebGLSampler* sampler, GLenum pname)
{
    if (isContextLost() || !validateWebGLObject("getSamplerParameter", sampler))
        return ScriptValue::createNull(scriptState);

    switch (pname) {
    case GL_TEXTURE_COMPARE_FUNC:
    case GL_TEXTURE_COMPARE_MODE:
    case GL_TEXTURE_MAG_FILTER:
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_WRAP_R:
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
        {
            GLint value = 0;
            webContext()->getSamplerParameteriv(objectOrZero(sampler), pname, &value);
            return WebGLAny(scriptState, static_cast<unsigned>(value));
        }
    case GL_TEXTURE_MAX_LOD:
    case GL_TEXTURE_MIN_LOD:
        {
            GLfloat value = 0.f;
            webContext()->getSamplerParameterfv(objectOrZero(sampler), pname, &value);
            return WebGLAny(scriptState, value);
        }
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getSamplerParameter", "invalid parameter name");
        return ScriptValue::createNull(scriptState);
    }
}

PassRefPtrWillBeRawPtr<WebGLSync> WebGL2RenderingContextBase::fenceSync(GLenum condition, GLbitfield flags)
{
    if (isContextLost())
        return nullptr;

    RefPtrWillBeRawPtr<WebGLSync> o = WebGLFenceSync::create(this, condition, flags);
    addSharedObject(o.get());
    return o.release();
}

GLboolean WebGL2RenderingContextBase::isSync(WebGLSync* sync)
{
    if (isContextLost() || !sync)
        return 0;

    return webContext()->isSync(sync->object());
}

void WebGL2RenderingContextBase::deleteSync(WebGLSync* sync)
{
    deleteObject(sync);
}

GLenum WebGL2RenderingContextBase::clientWaitSync(WebGLSync* sync, GLbitfield flags, GLuint timeout)
{
    if (isContextLost() || !validateWebGLObject("clientWaitSync", sync))
        return GL_WAIT_FAILED;

    GLuint64 timeout64 = (timeout == webGLTimeoutIgnored ? GL_TIMEOUT_IGNORED : timeout);
    return webContext()->clientWaitSync(syncObjectOrZero(sync), flags, timeout64);
}

void WebGL2RenderingContextBase::waitSync(WebGLSync* sync, GLbitfield flags, GLuint timeout)
{
    if (isContextLost() || !validateWebGLObject("waitSync", sync))
        return;

    GLuint64 timeout64 = (timeout == webGLTimeoutIgnored ? GL_TIMEOUT_IGNORED : timeout);
    webContext()->waitSync(syncObjectOrZero(sync), flags, timeout64);
}

ScriptValue WebGL2RenderingContextBase::getSyncParameter(ScriptState* scriptState, WebGLSync* sync, GLenum pname)
{
    if (isContextLost() || !validateWebGLObject("getSyncParameter", sync))
        return ScriptValue::createNull(scriptState);

    notImplemented();
    return ScriptValue::createNull(scriptState);
}

PassRefPtrWillBeRawPtr<WebGLTransformFeedback> WebGL2RenderingContextBase::createTransformFeedback()
{
    if (isContextLost())
        return nullptr;
    RefPtrWillBeRawPtr<WebGLTransformFeedback> o = WebGLTransformFeedback::create(this);
    addSharedObject(o.get());
    return o;
}

void WebGL2RenderingContextBase::deleteTransformFeedback(WebGLTransformFeedback* feedback)
{
    deleteObject(feedback);
}

GLboolean WebGL2RenderingContextBase::isTransformFeedback(WebGLTransformFeedback* feedback)
{
    if (isContextLost() || !feedback)
        return 0;

    return webContext()->isTransformFeedback(feedback->object());
}

void WebGL2RenderingContextBase::bindTransformFeedback(GLenum target, WebGLTransformFeedback* feedback)
{
    if (isContextLost() || !validateWebGLObject("bindTransformFeedback", feedback))
        return;

    webContext()->bindTransformFeedback(target, objectOrZero(feedback));
}

void WebGL2RenderingContextBase::beginTransformFeedback(GLenum primitiveMode)
{
    if (isContextLost())
        return;

    webContext()->beginTransformFeedback(primitiveMode);
}

void WebGL2RenderingContextBase::endTransformFeedback()
{
    if (isContextLost())
        return;

    webContext()->endTransformFeedback();
}

void WebGL2RenderingContextBase::transformFeedbackVaryings(WebGLProgram* program, const Vector<String>& varyings, GLenum bufferMode)
{
    if (isContextLost() || !validateWebGLObject("transformFeedbackVaryings", program))
        return;

    Vector<CString> keepAlive; // Must keep these instances alive while looking at their data
    Vector<const char*> varyingStrings;
    for (size_t i = 0; i < varyings.size(); ++i) {
        keepAlive.append(varyings[i].ascii());
        varyingStrings.append(keepAlive.last().data());
    }

    webContext()->transformFeedbackVaryings(objectOrZero(program), varyings.size(), varyingStrings.data(), bufferMode);
}

PassRefPtrWillBeRawPtr<WebGLActiveInfo> WebGL2RenderingContextBase::getTransformFeedbackVarying(WebGLProgram* program, GLuint index)
{
    if (isContextLost() || !validateWebGLObject("getTransformFeedbackVarying", program))
        return nullptr;

    notImplemented();
    return nullptr;
}

void WebGL2RenderingContextBase::pauseTransformFeedback()
{
    if (isContextLost())
        return;

    webContext()->pauseTransformFeedback();
}

void WebGL2RenderingContextBase::resumeTransformFeedback()
{
    if (isContextLost())
        return;

    webContext()->resumeTransformFeedback();
}

void WebGL2RenderingContextBase::bindBufferBase(GLenum target, GLuint index, WebGLBuffer* buffer)
{
    if (isContextLost() || !validateWebGLObject("bindBufferBase", buffer))
        return;

    webContext()->bindBufferBase(target, index, objectOrZero(buffer));
}

void WebGL2RenderingContextBase::bindBufferRange(GLenum target, GLuint index, WebGLBuffer* buffer, GLintptr offset, GLsizeiptr size)
{
    if (isContextLost() || !validateWebGLObject("bindBufferRange", buffer))
        return;

    webContext()->bindBufferRange(target, index, objectOrZero(buffer), offset, size);
}

ScriptValue WebGL2RenderingContextBase::getIndexedParameter(ScriptState* scriptState, GLenum target, GLuint index)
{
    if (isContextLost())
        return ScriptValue::createNull(scriptState);

    notImplemented();
    return ScriptValue::createNull(scriptState);
}

Vector<GLuint> WebGL2RenderingContextBase::getUniformIndices(WebGLProgram* program, const Vector<String>& uniformNames)
{
    Vector<GLuint> result;
    if (isContextLost() || !validateWebGLObject("getUniformIndices", program))
        return result;

    notImplemented();
    // FIXME: copy uniform names into array of const char*
    /*result.resize(uniformNames.size());
    webContext()->getUniformIndices(objectOrZero(program), uniformNames.size(), uniformNames.data(), result.data());*/
    return result;
}

Vector<GLint> WebGL2RenderingContextBase::getActiveUniforms(WebGLProgram* program, const Vector<GLuint>& uniformIndices, GLenum pname)
{
    Vector<GLint> result;
    if (isContextLost() || !validateWebGLObject("getActiveUniforms", program))
        return result;

    result.resize(uniformIndices.size());
    webContext()->getActiveUniformsiv(objectOrZero(program), uniformIndices.size(), uniformIndices.data(), pname, result.data());
    return result;
}

GLuint WebGL2RenderingContextBase::getUniformBlockIndex(WebGLProgram* program, const String& uniformBlockName)
{
    if (isContextLost() || !validateWebGLObject("getUniformBlockIndex", program))
        return 0;
    if (!validateString("getUniformBlockIndex", uniformBlockName))
        return 0;

    return webContext()->getUniformBlockIndex(objectOrZero(program), uniformBlockName.utf8().data());
}

ScriptValue WebGL2RenderingContextBase::getActiveUniformBlockParameter(ScriptState* scriptState, WebGLProgram* program, GLuint uniformBlockIndex, GLenum pname)
{
    if (isContextLost() || !validateWebGLObject("getActiveUniformBlockParameter", program))
        return ScriptValue::createNull(scriptState);

    switch (pname) {
    case GL_UNIFORM_BLOCK_BINDING:
    case GL_UNIFORM_BLOCK_DATA_SIZE:
    case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS: {
        GLint intValue = 0;
        webContext()->getActiveUniformBlockiv(objectOrZero(program), uniformBlockIndex, pname, &intValue);
        return WebGLAny(scriptState, static_cast<unsigned>(intValue));
    }
    case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES: {
        GLint uniformCount = 0;
        webContext()->getActiveUniformBlockiv(objectOrZero(program), uniformBlockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &uniformCount);

        Vector<GLint> signedIndices(uniformCount);
        Vector<GLuint> indices(uniformCount);
        webContext()->getActiveUniformBlockiv(objectOrZero(program), uniformBlockIndex, pname, signedIndices.data());
        for (GLint i = 0; i < uniformCount; ++i) {
            indices[i] = static_cast<unsigned>(signedIndices[i]);
        }

        return WebGLAny(scriptState, DOMUint32Array::create(indices.data(), indices.size()));
    }
    case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
    case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER: {
        GLint boolValue = 0;
        webContext()->getActiveUniformBlockiv(objectOrZero(program), uniformBlockIndex, pname, &boolValue);
        return WebGLAny(scriptState, static_cast<bool>(boolValue));
    }
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getActiveUniformBlockParameter", "invalid pname");
        return ScriptValue::createNull(scriptState);
    }
}

String WebGL2RenderingContextBase::getActiveUniformBlockName(WebGLProgram* program, GLuint uniformBlockIndex)
{
    if (isContextLost() || !validateWebGLObject("getActiveUniformBlockName", program))
        return String();

    notImplemented();
    return String();
}

void WebGL2RenderingContextBase::uniformBlockBinding(WebGLProgram* program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    if (isContextLost() || !validateWebGLObject("uniformBlockBinding", program))
        return;

    webContext()->uniformBlockBinding(objectOrZero(program), uniformBlockIndex, uniformBlockBinding);
}

PassRefPtrWillBeRawPtr<WebGLVertexArrayObjectOES> WebGL2RenderingContextBase::createVertexArray()
{
    if (isContextLost())
        return nullptr;

    RefPtrWillBeRawPtr<WebGLVertexArrayObjectOES> o = WebGLVertexArrayObjectOES::create(this, WebGLVertexArrayObjectOES::VaoTypeUser);
    addContextObject(o.get());
    return o.release();
}

void WebGL2RenderingContextBase::deleteVertexArray(WebGLVertexArrayObjectOES* vertexArray)
{
    if (isContextLost() || !vertexArray)
        return;

    if (!vertexArray->isDefaultObject() && vertexArray == m_boundVertexArrayObject)
        setBoundVertexArrayObject(nullptr);

    vertexArray->deleteObject(webContext());
}

GLboolean WebGL2RenderingContextBase::isVertexArray(WebGLVertexArrayObjectOES* vertexArray)
{
    if (isContextLost() || !vertexArray)
        return 0;

    if (!vertexArray->hasEverBeenBound())
        return 0;

    return webContext()->isVertexArrayOES(vertexArray->object());
}

void WebGL2RenderingContextBase::bindVertexArray(WebGLVertexArrayObjectOES* vertexArray)
{
    if (isContextLost())
        return;

    if (vertexArray && (vertexArray->isDeleted() || !vertexArray->validate(0, this))) {
        webContext()->synthesizeGLError(GL_INVALID_OPERATION);
        return;
    }

    if (vertexArray && !vertexArray->isDefaultObject() && vertexArray->object()) {
        webContext()->bindVertexArrayOES(objectOrZero(vertexArray));

        vertexArray->setHasEverBeenBound();
        setBoundVertexArrayObject(vertexArray);
    } else {
        webContext()->bindVertexArrayOES(0);
        setBoundVertexArrayObject(nullptr);
    }
}

bool WebGL2RenderingContextBase::validateCapability(const char* functionName, GLenum cap)
{
    switch (cap) {
    case GL_RASTERIZER_DISCARD:
        return true;
    default:
        return WebGLRenderingContextBase::validateCapability(functionName, cap);
    }
}

bool WebGL2RenderingContextBase::validateAndUpdateBufferBindTarget(const char* functionName, GLenum target, WebGLBuffer* buffer)
{
    if (buffer && buffer->getTarget() && (buffer->getTarget() == GL_ELEMENT_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) && buffer->getTarget() != target) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "element array buffers can not be bound to a different target");
        return false;
    }

    switch (target) {
    case GL_ARRAY_BUFFER:
        m_boundArrayBuffer = buffer;
        break;
    case GL_ELEMENT_ARRAY_BUFFER:
        m_boundVertexArrayObject->setElementArrayBuffer(buffer);
        break;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
    case GL_COPY_READ_BUFFER:
    case GL_COPY_WRITE_BUFFER:
    case GL_PIXEL_PACK_BUFFER:
    case GL_PIXEL_UNPACK_BUFFER:
    case GL_UNIFORM_BUFFER:
        // FIXME: Some of these ES3 buffer targets may require additional state tracking.
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid target");
        return false;
    }

    return true;
}

DEFINE_TRACE(WebGL2RenderingContextBase)
{
    WebGLRenderingContextBase::trace(visitor);
}

} // namespace blink
