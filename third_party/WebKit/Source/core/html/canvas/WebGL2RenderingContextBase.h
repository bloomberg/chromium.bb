// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGL2RenderingContextBase_h
#define WebGL2RenderingContextBase_h

#include "core/html/canvas/WebGLExtension.h"
#include "core/html/canvas/WebGLRenderingContextBase.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class WebGLTexture;

class WebGLActiveInfo;
class WebGLBuffer;
class WebGLProgram;
class WebGLQuery;
class WebGLSampler;
class WebGLSync;
class WebGLTransformFeedback;
class WebGLUniformLocation;
class WebGLVertexArrayObject;

class WebGL2RenderingContextBase : public WebGLRenderingContextBase {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN_NESTED(WebGL2RenderingContextBase, WebGLRenderingContextBase);
public:
    virtual ~WebGL2RenderingContextBase();

    /* Buffer objects */
    void copyBufferSubData(GLenum, GLenum, GLintptr, GLintptr, GLsizeiptr);
    void getBufferSubData(GLenum target, GLintptr offset, DOMArrayBuffer* returnedData);

    /* Framebuffer objects */
    void blitFramebuffer(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
    void framebufferTextureLayer(GLenum, GLenum, const WebGLTexture*, GLint, GLint);
    ScriptValue getInternalformatParameter(ScriptState*, GLenum, GLenum, GLenum);
    void invalidateFramebuffer(GLenum, Vector<GLenum>&);
    void invalidateSubFramebuffer(GLenum, Vector<GLenum>&, GLint, GLint, GLsizei, GLsizei);
    void readBuffer(GLenum);

    /* Renderbuffer objects */
    void renderbufferStorageMultisample(GLenum, GLsizei, GLenum, GLsizei, GLsizei);

    /* Texture objects */
    void texStorage2D(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
    void texStorage3D(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei);
    void texImage3D(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, DOMArrayBufferView*);
    void texSubImage3D(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, DOMArrayBufferView*);
    void texSubImage3D(GLenum, GLint, GLint, GLint, GLint, GLenum, GLenum, ImageData*);
    void texSubImage3D(GLenum, GLint, GLint, GLint, GLint, GLenum, GLenum, HTMLImageElement*);
    void texSubImage3D(GLenum, GLint, GLint, GLint, GLint, GLenum, GLenum, HTMLCanvasElement*);
    void texSubImage3D(GLenum, GLint, GLint, GLint, GLint, GLenum, GLenum, HTMLVideoElement*);
    void copyTexSubImage3D(GLenum, GLint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
    void compressedTexImage3D(GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, DOMArrayBufferView*);
    void compressedTexSubImage3D(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, DOMArrayBufferView*);

    /* Programs and shaders */
    GLint getFragDataLocation(WebGLProgram*, const String&);

    /* Uniforms and attributes */
    void uniform1ui(const WebGLUniformLocation*, GLuint);
    void uniform2ui(const WebGLUniformLocation*, GLuint, GLuint);
    void uniform3ui(const WebGLUniformLocation*, GLuint, GLuint, GLuint);
    void uniform4ui(const WebGLUniformLocation*, GLuint, GLuint, GLuint, GLuint);
    void uniform1uiv(const WebGLUniformLocation*, Vector<GLuint>&);
    void uniform2uiv(const WebGLUniformLocation*, Vector<GLuint>&);
    void uniform3uiv(const WebGLUniformLocation*, Vector<GLuint>&);
    void uniform4uiv(const WebGLUniformLocation*, Vector<GLuint>&);
    void uniformMatrix2x3fv(const WebGLUniformLocation*, GLboolean, DOMFloat32Array*);
    void uniformMatrix2x3fv(const WebGLUniformLocation*, GLboolean, Vector<GLfloat>&);
    void uniformMatrix3x2fv(const WebGLUniformLocation*, GLboolean, DOMFloat32Array*);
    void uniformMatrix3x2fv(const WebGLUniformLocation*, GLboolean, Vector<GLfloat>&);
    void uniformMatrix2x4fv(const WebGLUniformLocation*, GLboolean, DOMFloat32Array*);
    void uniformMatrix2x4fv(const WebGLUniformLocation*, GLboolean, Vector<GLfloat>&);
    void uniformMatrix4x2fv(const WebGLUniformLocation*, GLboolean, DOMFloat32Array*);
    void uniformMatrix4x2fv(const WebGLUniformLocation*, GLboolean, Vector<GLfloat>&);
    void uniformMatrix3x4fv(const WebGLUniformLocation*, GLboolean, DOMFloat32Array*);
    void uniformMatrix3x4fv(const WebGLUniformLocation*, GLboolean, Vector<GLfloat>&);
    void uniformMatrix4x3fv(const WebGLUniformLocation*, GLboolean, DOMFloat32Array*);
    void uniformMatrix4x3fv(const WebGLUniformLocation*, GLboolean, Vector<GLfloat>&);

    void vertexAttribI4i(GLuint, GLint, GLint, GLint, GLint);
    void vertexAttribI4iv(GLuint, const Vector<GLint>&);
    void vertexAttribI4ui(GLuint, GLuint, GLuint, GLuint, GLuint);
    void vertexAttribI4uiv(GLuint, const Vector<GLuint>&);
    void vertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset);

    /* Writing to the drawing buffer */
    void vertexAttribDivisor(GLuint, GLuint);
    void drawArraysInstanced(GLenum, GLint, GLsizei, GLsizei);
    void drawElementsInstanced(GLenum, GLsizei, GLenum, GLintptr, GLsizei);
    void drawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, GLintptr offset);

    /* Multiple Render Targets */
    void drawBuffers(const Vector<GLenum>&);
    void clearBufferiv(GLenum, GLint, DOMInt32Array*);
    void clearBufferiv(GLenum, GLint, const Vector<GLint>&);
    void clearBufferuiv(GLenum, GLint, DOMUint32Array*);
    void clearBufferuiv(GLenum, GLint, const Vector<GLuint>&);
    void clearBufferfv(GLenum, GLint, DOMFloat32Array*);
    void clearBufferfv(GLenum, GLint, const Vector<GLfloat>&);
    void clearBufferfi(GLenum, GLint, GLfloat, GLint);

    /* Query Objects */
    PassRefPtrWillBeRawPtr<WebGLQuery> createQuery();
    void deleteQuery(WebGLQuery*);
    GLboolean isQuery(WebGLQuery*);
    void beginQuery(GLenum, WebGLQuery*);
    void endQuery(GLenum);
    PassRefPtrWillBeRawPtr<WebGLQuery> getQuery(GLenum, GLenum);
    ScriptValue getQueryParameter(ScriptState*, WebGLQuery*, GLenum);

    /* Sampler Objects */
    PassRefPtrWillBeRawPtr<WebGLSampler> createSampler();
    void deleteSampler(WebGLSampler*);
    GLboolean isSampler(WebGLSampler*);
    void bindSampler(GLuint, WebGLSampler*);
    void samplerParameteri(WebGLSampler*, GLenum, GLint);
    void samplerParameterf(WebGLSampler*, GLenum, GLfloat);
    ScriptValue getSamplerParameter(ScriptState*, WebGLSampler*, GLenum);

    /* Sync objects */
    PassRefPtrWillBeRawPtr<WebGLSync> fenceSync(GLenum, GLbitfield);
    GLboolean isSync(WebGLSync*);
    void deleteSync(WebGLSync*);
    GLenum clientWaitSync(WebGLSync*, GLbitfield, GLuint);
    void waitSync(WebGLSync*, GLbitfield, GLuint);

    ScriptValue getSyncParameter(ScriptState*, WebGLSync*, GLenum);

    /* Transform Feedback */
    PassRefPtrWillBeRawPtr<WebGLTransformFeedback> createTransformFeedback();
    void deleteTransformFeedback(WebGLTransformFeedback*);
    GLboolean isTransformFeedback(WebGLTransformFeedback*);
    void bindTransformFeedback(GLenum, WebGLTransformFeedback*);
    void beginTransformFeedback(GLenum);
    void endTransformFeedback();
    void transformFeedbackVaryings(WebGLProgram*, const Vector<String>&, GLenum);
    PassRefPtrWillBeRawPtr<WebGLActiveInfo> getTransformFeedbackVarying(WebGLProgram*, GLuint);
    void pauseTransformFeedback();
    void resumeTransformFeedback();

    /* Uniform Buffer Objects and Transform Feedback Buffers */
    void bindBufferBase(GLenum, GLuint, WebGLBuffer*);
    void bindBufferRange(GLenum, GLuint, WebGLBuffer*, GLintptr, GLsizeiptr);
    ScriptValue getIndexedParameter(ScriptState*, GLenum, GLuint);
    Vector<GLuint> getUniformIndices(WebGLProgram*, const Vector<String>&);
    Vector<GLint> getActiveUniforms(WebGLProgram*, const Vector<GLuint>&, GLenum);
    GLuint getUniformBlockIndex(WebGLProgram*, const String&);
    ScriptValue getActiveUniformBlockParameter(ScriptState*, WebGLProgram*, GLuint, GLenum);
    String getActiveUniformBlockName(WebGLProgram*, GLuint);
    void uniformBlockBinding(WebGLProgram*, GLuint, GLuint);

    /* Vertex Array Objects */
    PassRefPtrWillBeRawPtr<WebGLVertexArrayObjectOES> createVertexArray();
    void deleteVertexArray(WebGLVertexArrayObjectOES*);
    GLboolean isVertexArray(WebGLVertexArrayObjectOES*);
    void bindVertexArray(WebGLVertexArrayObjectOES*);

    DECLARE_VIRTUAL_TRACE();

protected:
    WebGL2RenderingContextBase(HTMLCanvasElement*, PassOwnPtr<blink::WebGraphicsContext3D>, const WebGLContextAttributes& requestedAttributes);

    bool validateClearBuffer(const char* functionName, GLenum buffer, GLsizei length);

    /* WebGLRenderingContextBase overrides */
    bool validateCapability(const char* functionName, GLenum) override;
    bool validateAndUpdateBufferBindTarget(const char* functionName, GLenum, WebGLBuffer*) override;
};

DEFINE_TYPE_CASTS(WebGL2RenderingContextBase, CanvasRenderingContext, context,
    context->is3d() && WebGLRenderingContextBase::getWebGLVersion(context) >= 2,
    context.is3d() && WebGLRenderingContextBase::getWebGLVersion(&context) >= 2);

} // namespace blink

#endif
