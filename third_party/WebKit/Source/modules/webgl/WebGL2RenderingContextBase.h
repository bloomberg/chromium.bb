// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGL2RenderingContextBase_h
#define WebGL2RenderingContextBase_h

#include "modules/webgl/WebGLExtension.h"
#include "modules/webgl/WebGLRenderingContextBase.h"

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
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(WebGL2RenderingContextBase);
public:
    ~WebGL2RenderingContextBase() override;

    /* Buffer objects */
    void copyBufferSubData(GLenum, GLenum, long long, long long, long long);
    void getBufferSubData(GLenum target, long long offset, DOMArrayBuffer* returnedData);

    /* Framebuffer objects */
    bool validateTexFuncLayer(const char*, GLenum texTarget, GLint layer);
    void blitFramebuffer(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
    void framebufferTextureLayer(ScriptState*, GLenum, GLenum, WebGLTexture*, GLint, GLint);
    ScriptValue getInternalformatParameter(ScriptState*, GLenum, GLenum, GLenum);
    void invalidateFramebuffer(GLenum, const Vector<GLenum>&);
    void invalidateSubFramebuffer(GLenum, const Vector<GLenum>&, GLint, GLint, GLsizei, GLsizei);
    void readBuffer(GLenum);

    /* Renderbuffer objects */
    void renderbufferStorageMultisample(GLenum, GLsizei, GLenum, GLsizei, GLsizei);

    /* Texture objects */
    void texStorage2D(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
    void texStorage3D(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei);
    void texImage3D(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, DOMArrayBufferView*);
    void texSubImage3D(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, DOMArrayBufferView*);
    void texSubImage3D(GLenum, GLint, GLint, GLint, GLint, GLenum, GLenum, ImageData*);
    void texSubImage3D(GLenum, GLint, GLint, GLint, GLint, GLenum, GLenum, HTMLImageElement*, ExceptionState&);
    void texSubImage3D(GLenum, GLint, GLint, GLint, GLint, GLenum, GLenum, HTMLCanvasElement*, ExceptionState&);
    void texSubImage3D(GLenum, GLint, GLint, GLint, GLint, GLenum, GLenum, HTMLVideoElement*, ExceptionState&);
    void texSubImage3D(GLenum, GLint, GLint, GLint, GLint, GLenum, GLenum, PassRefPtrWillBeRawPtr<ImageBitmap>, ExceptionState&);
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
    void uniform1uiv(const WebGLUniformLocation*, const FlexibleUint32ArrayView&);
    void uniform1uiv(const WebGLUniformLocation*, Vector<GLuint>&);
    void uniform2uiv(const WebGLUniformLocation*, const FlexibleUint32ArrayView&);
    void uniform2uiv(const WebGLUniformLocation*, Vector<GLuint>&);
    void uniform3uiv(const WebGLUniformLocation*, const FlexibleUint32ArrayView&);
    void uniform3uiv(const WebGLUniformLocation*, Vector<GLuint>&);
    void uniform4uiv(const WebGLUniformLocation*, const FlexibleUint32ArrayView&);
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
    void vertexAttribI4iv(GLuint, const DOMInt32Array*);
    void vertexAttribI4iv(GLuint, const Vector<GLint>&);
    void vertexAttribI4ui(GLuint, GLuint, GLuint, GLuint, GLuint);
    void vertexAttribI4uiv(GLuint, const DOMUint32Array*);
    void vertexAttribI4uiv(GLuint, const Vector<GLuint>&);
    void vertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, long long offset);

    /* Writing to the drawing buffer */
    void vertexAttribDivisor(GLuint, GLuint);
    void drawArraysInstanced(GLenum, GLint, GLsizei, GLsizei);
    void drawElementsInstanced(GLenum, GLsizei, GLenum, long long, GLsizei);
    void drawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, long long offset);

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
    WebGLQuery* createQuery();
    void deleteQuery(WebGLQuery*);
    GLboolean isQuery(WebGLQuery*);
    void beginQuery(GLenum, WebGLQuery*);
    void endQuery(GLenum);
    WebGLQuery* getQuery(GLenum, GLenum);
    ScriptValue getQueryParameter(ScriptState*, WebGLQuery*, GLenum);

    /* Sampler Objects */
    WebGLSampler* createSampler();
    void deleteSampler(WebGLSampler*);
    GLboolean isSampler(WebGLSampler*);
    void bindSampler(GLuint, WebGLSampler*);
    void samplerParameteri(WebGLSampler*, GLenum, GLint);
    void samplerParameterf(WebGLSampler*, GLenum, GLfloat);
    ScriptValue getSamplerParameter(ScriptState*, WebGLSampler*, GLenum);

    /* Sync objects */
    WebGLSync* fenceSync(GLenum, GLbitfield);
    GLboolean isSync(WebGLSync*);
    void deleteSync(WebGLSync*);
    GLenum clientWaitSync(WebGLSync*, GLbitfield, GLint64);
    void waitSync(WebGLSync*, GLbitfield, GLint64);

    ScriptValue getSyncParameter(ScriptState*, WebGLSync*, GLenum);

    /* Transform Feedback */
    WebGLTransformFeedback* createTransformFeedback();
    void deleteTransformFeedback(WebGLTransformFeedback*);
    GLboolean isTransformFeedback(WebGLTransformFeedback*);
    void bindTransformFeedback(GLenum, WebGLTransformFeedback*);
    void beginTransformFeedback(GLenum);
    void endTransformFeedback();
    void transformFeedbackVaryings(WebGLProgram*, const Vector<String>&, GLenum);
    WebGLActiveInfo* getTransformFeedbackVarying(WebGLProgram*, GLuint);
    void pauseTransformFeedback();
    void resumeTransformFeedback();
    bool validateTransformFeedbackPrimitiveMode(const char* functionName, GLenum primitiveMode);

    /* Uniform Buffer Objects and Transform Feedback Buffers */
    void bindBufferBase(GLenum, GLuint, WebGLBuffer*);
    void bindBufferRange(GLenum, GLuint, WebGLBuffer*, long long, long long);
    ScriptValue getIndexedParameter(ScriptState*, GLenum, GLuint);
    Vector<GLuint> getUniformIndices(WebGLProgram*, const Vector<String>&);
    ScriptValue getActiveUniforms(ScriptState*, WebGLProgram*, const Vector<GLuint>&, GLenum);
    GLuint getUniformBlockIndex(WebGLProgram*, const String&);
    ScriptValue getActiveUniformBlockParameter(ScriptState*, WebGLProgram*, GLuint, GLenum);
    String getActiveUniformBlockName(WebGLProgram*, GLuint);
    void uniformBlockBinding(WebGLProgram*, GLuint, GLuint);

    /* Vertex Array Objects */
    WebGLVertexArrayObject* createVertexArray();
    void deleteVertexArray(ScriptState*, WebGLVertexArrayObject*);
    GLboolean isVertexArray(WebGLVertexArrayObject*);
    void bindVertexArray(ScriptState*, WebGLVertexArrayObject*);

    /* Reading */
    void readPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, long long offset);

    /* WebGLRenderingContextBase overrides */
    void initializeNewContext() override;
    void bindFramebuffer(ScriptState*, GLenum target, WebGLFramebuffer*) override;
    void deleteFramebuffer(WebGLFramebuffer*) override;
    ScriptValue getParameter(ScriptState*, GLenum pname) override;
    ScriptValue getTexParameter(ScriptState*, GLenum target, GLenum pname) override;
    ScriptValue getFramebufferAttachmentParameter(ScriptState*, GLenum target, GLenum attachment, GLenum pname) override;
    void pixelStorei(GLenum pname, GLint param) override;
    void readPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, DOMArrayBufferView* pixels) override;
    void restoreCurrentFramebuffer() override;

    EAGERLY_FINALIZE();
    DECLARE_VIRTUAL_TRACE();

protected:
    WebGL2RenderingContextBase(HTMLCanvasElement*, PassOwnPtr<WebGraphicsContext3D>, const WebGLContextAttributes& requestedAttributes);

    // Helper function to validate target and the attachment combination for getFramebufferAttachmentParameters.
    // Generate GL error and return false if parameters are illegal.
    bool validateGetFramebufferAttachmentParameterFunc(const char* functionName, GLenum target, GLenum attachment);

    bool validateClearBuffer(const char* functionName, GLenum buffer, GLsizei length);

    enum TexStorageType {
        TexStorageType2D,
        TexStorageType3D,
    };
    bool validateTexStorage(const char*, GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, TexStorageType);

    bool validateUniformBlockIndex(const char*, WebGLProgram*, GLuint);

    ScriptValue getInt64Parameter(ScriptState*, GLenum);

    void texSubImage3DImpl(GLenum, GLint, GLint, GLint, GLint, GLenum, GLenum, Image*, WebGLImageConversion::ImageHtmlDomSource, bool, bool);
    void samplerParameter(WebGLSampler*, GLenum, GLfloat, GLint, bool);

    bool isBufferBoundToTransformFeedback(WebGLBuffer*);
    bool isBufferBoundToNonTransformFeedback(WebGLBuffer*);
    bool validateBufferTargetCompatibility(const char*, GLenum, WebGLBuffer*);

    bool validateBufferBaseTarget(const char* functionName, GLenum target);
    bool validateAndUpdateBufferBindBaseTarget(const char* functionName, GLenum, GLuint, WebGLBuffer*);

    WebGLImageConversion::PixelStoreParams getPackPixelStoreParams() override;
    WebGLImageConversion::PixelStoreParams getUnpackPixelStoreParams() override;

    bool checkAndTranslateAttachments(const char* functionName, GLenum, const Vector<GLenum>&, Vector<GLenum>&);

    /* WebGLRenderingContextBase overrides */
    unsigned getMaxWebGLLocationLength() const override { return 1024; };
    bool validateCapability(const char* functionName, GLenum) override;
    bool validateBufferTarget(const char* functionName, GLenum target) override;
    bool validateAndUpdateBufferBindTarget(const char* functionName, GLenum, WebGLBuffer*) override;
    WebGLTexture* validateTextureBinding(const char* functionName, GLenum target, bool useSixEnumsForCubeMap) override;
    bool validateFramebufferTarget(GLenum target) override;
    bool validateReadPixelsFormatAndType(GLenum format, GLenum type) override;

    DOMArrayBufferView::ViewType readPixelsExpectedArrayBufferViewType(GLenum type) override;
    WebGLFramebuffer* getFramebufferBinding(GLenum target) override;
    WebGLFramebuffer* getReadFramebufferBinding() override;
    GLint getMaxTextureLevelForTarget(GLenum target) override;
    void renderbufferStorageImpl(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, const char* functionName) override;
    const WebGLSamplerState* getTextureUnitSamplerState(GLenum target, GLuint unit) const override;

    // Helper function to validate the target for compressedTex{Sub}Image3D.
    bool validateTexFunc3DTarget(const char* functionName, GLenum target);

    WebGLBuffer* validateBufferDataTarget(const char* functionName, GLenum target) override;
    bool validateBufferDataUsage(const char* functionName, GLenum usage) override;

    void removeBoundBuffer(WebGLBuffer*) override;

    void resetUnpackParameters() override;
    void restoreUnpackParameters() override;

    bool transformFeedbackActive() const override;
    bool transformFeedbackPaused() const override;
    void setTransformFeedbackActive(bool);
    void setTransformFeedbackPaused(bool);

    PersistentWillBeMember<WebGLFramebuffer> m_readFramebufferBinding;
    PersistentWillBeMember<WebGLTransformFeedback> m_transformFeedbackBinding;

    std::set<GLenum> m_supportedInternalFormatsStorage;
    std::set<GLenum> m_compressedTextureFormatsETC2EAC;

    PersistentWillBeMember<WebGLBuffer> m_boundCopyReadBuffer;
    PersistentWillBeMember<WebGLBuffer> m_boundCopyWriteBuffer;
    PersistentWillBeMember<WebGLBuffer> m_boundPixelPackBuffer;
    PersistentWillBeMember<WebGLBuffer> m_boundPixelUnpackBuffer;
    PersistentWillBeMember<WebGLBuffer> m_boundTransformFeedbackBuffer;
    PersistentWillBeMember<WebGLBuffer> m_boundUniformBuffer;

    PersistentHeapVectorWillBeHeapVector<Member<WebGLBuffer>> m_boundIndexedTransformFeedbackBuffers;
    PersistentHeapVectorWillBeHeapVector<Member<WebGLBuffer>> m_boundIndexedUniformBuffers;
    GLint m_maxTransformFeedbackSeparateAttribs;
    size_t m_maxBoundUniformBufferIndex;

    PersistentWillBeMember<WebGLQuery> m_currentBooleanOcclusionQuery;
    PersistentWillBeMember<WebGLQuery> m_currentTransformFeedbackPrimitivesWrittenQuery;
    PersistentHeapVectorWillBeHeapVector<Member<WebGLSampler>> m_samplerUnits;

    GLint m_packRowLength;
    GLint m_packSkipPixels;
    GLint m_packSkipRows;
    GLint m_unpackRowLength;
    GLint m_unpackImageHeight;
    GLint m_unpackSkipPixels;
    GLint m_unpackSkipRows;
    GLint m_unpackSkipImages;
};

DEFINE_TYPE_CASTS(WebGL2RenderingContextBase, CanvasRenderingContext, context,
    context->is3d() && WebGLRenderingContextBase::getWebGLVersion(context) >= 2,
    context.is3d() && WebGLRenderingContextBase::getWebGLVersion(&context) >= 2);

} // namespace blink

#endif
