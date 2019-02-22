//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// State.h: Defines the State class, encapsulating raw GL state

#ifndef LIBANGLE_STATE_H_
#define LIBANGLE_STATE_H_

#include <bitset>
#include <memory>

#include "common/Color.h"
#include "common/angleutils.h"
#include "common/bitset_utils.h"
#include "libANGLE/Debug.h"
#include "libANGLE/GLES1State.h"
#include "libANGLE/Program.h"
#include "libANGLE/ProgramPipeline.h"
#include "libANGLE/RefCountObject.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Sampler.h"
#include "libANGLE/Texture.h"
#include "libANGLE/TransformFeedback.h"
#include "libANGLE/Version.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/angletypes.h"

namespace gl
{
class Query;
class VertexArray;
class Context;
struct Caps;

class State : angle::NonCopyable
{
  public:
    State(bool debug,
          bool bindGeneratesResource,
          bool clientArraysEnabled,
          bool robustResourceInit,
          bool programBinaryCacheEnabled);
    ~State();

    void initialize(Context *context);
    void reset(const Context *context);

    // State chunk getters
    const RasterizerState &getRasterizerState() const;
    const BlendState &getBlendState() const;
    const DepthStencilState &getDepthStencilState() const;

    // Clear behavior setters & state parameter block generation function
    void setColorClearValue(float red, float green, float blue, float alpha);
    void setDepthClearValue(float depth);
    void setStencilClearValue(int stencil);

    const ColorF &getColorClearValue() const { return mColorClearValue; }
    float getDepthClearValue() const { return mDepthClearValue; }
    int getStencilClearValue() const { return mStencilClearValue; }

    // Write mask manipulation
    void setColorMask(bool red, bool green, bool blue, bool alpha);
    void setDepthMask(bool mask);

    // Discard toggle & query
    bool isRasterizerDiscardEnabled() const;
    void setRasterizerDiscard(bool enabled);

    // Primitive restart
    bool isPrimitiveRestartEnabled() const;
    void setPrimitiveRestart(bool enabled);

    // Face culling state manipulation
    bool isCullFaceEnabled() const;
    void setCullFace(bool enabled);
    void setCullMode(CullFaceMode mode);
    void setFrontFace(GLenum front);

    // Depth test state manipulation
    bool isDepthTestEnabled() const;
    void setDepthTest(bool enabled);
    void setDepthFunc(GLenum depthFunc);
    void setDepthRange(float zNear, float zFar);
    float getNearPlane() const;
    float getFarPlane() const;

    // Blend state manipulation
    bool isBlendEnabled() const;
    void setBlend(bool enabled);
    void setBlendFactors(GLenum sourceRGB, GLenum destRGB, GLenum sourceAlpha, GLenum destAlpha);
    void setBlendColor(float red, float green, float blue, float alpha);
    void setBlendEquation(GLenum rgbEquation, GLenum alphaEquation);
    const ColorF &getBlendColor() const;

    // Stencil state maniupulation
    bool isStencilTestEnabled() const;
    void setStencilTest(bool enabled);
    void setStencilParams(GLenum stencilFunc, GLint stencilRef, GLuint stencilMask);
    void setStencilBackParams(GLenum stencilBackFunc, GLint stencilBackRef, GLuint stencilBackMask);
    void setStencilWritemask(GLuint stencilWritemask);
    void setStencilBackWritemask(GLuint stencilBackWritemask);
    void setStencilOperations(GLenum stencilFail,
                              GLenum stencilPassDepthFail,
                              GLenum stencilPassDepthPass);
    void setStencilBackOperations(GLenum stencilBackFail,
                                  GLenum stencilBackPassDepthFail,
                                  GLenum stencilBackPassDepthPass);
    GLint getStencilRef() const;
    GLint getStencilBackRef() const;

    // Depth bias/polygon offset state manipulation
    bool isPolygonOffsetFillEnabled() const;
    void setPolygonOffsetFill(bool enabled);
    void setPolygonOffsetParams(GLfloat factor, GLfloat units);

    // Multisample coverage state manipulation
    bool isSampleAlphaToCoverageEnabled() const;
    void setSampleAlphaToCoverage(bool enabled);
    bool isSampleCoverageEnabled() const;
    void setSampleCoverage(bool enabled);
    void setSampleCoverageParams(GLclampf value, bool invert);
    GLfloat getSampleCoverageValue() const;
    bool getSampleCoverageInvert() const;

    // Multisample mask state manipulation.
    bool isSampleMaskEnabled() const;
    void setSampleMaskEnabled(bool enabled);
    void setSampleMaskParams(GLuint maskNumber, GLbitfield mask);
    GLbitfield getSampleMaskWord(GLuint maskNumber) const;
    GLuint getMaxSampleMaskWords() const;

    // Multisampling/alpha to one manipulation.
    void setSampleAlphaToOne(bool enabled);
    bool isSampleAlphaToOneEnabled() const;
    void setMultisampling(bool enabled);
    bool isMultisamplingEnabled() const;

    // Scissor test state toggle & query
    bool isScissorTestEnabled() const;
    void setScissorTest(bool enabled);
    void setScissorParams(GLint x, GLint y, GLsizei width, GLsizei height);
    const Rectangle &getScissor() const;

    // Dither state toggle & query
    bool isDitherEnabled() const;
    void setDither(bool enabled);

    // Generic state toggle & query
    void setEnableFeature(GLenum feature, bool enabled);
    bool getEnableFeature(GLenum feature) const;

    // Line width state setter
    void setLineWidth(GLfloat width);
    float getLineWidth() const;

    // Hint setters
    void setGenerateMipmapHint(GLenum hint);
    void setFragmentShaderDerivativeHint(GLenum hint);

    // GL_CHROMIUM_bind_generates_resource
    bool isBindGeneratesResourceEnabled() const { return mBindGeneratesResource; }

    // GL_ANGLE_client_arrays
    bool areClientArraysEnabled() const;

    // Viewport state setter/getter
    void setViewportParams(GLint x, GLint y, GLsizei width, GLsizei height);
    const Rectangle &getViewport() const;

    // Texture binding & active texture unit manipulation
    void setActiveSampler(unsigned int active);
    unsigned int getActiveSampler() const;
    Error setSamplerTexture(const Context *context, TextureType type, Texture *texture);
    Texture *getTargetTexture(TextureType type) const;

    Texture *getSamplerTexture(unsigned int sampler, TextureType type) const
    {
        ASSERT(sampler < mSamplerTextures[type].size());
        return mSamplerTextures[type][sampler].get();
    }

    GLuint getSamplerTextureId(unsigned int sampler, TextureType type) const;
    void detachTexture(const Context *context, const TextureMap &zeroTextures, GLuint texture);
    void initializeZeroTextures(const Context *context, const TextureMap &zeroTextures);

    // Sampler object binding manipulation
    void setSamplerBinding(const Context *context, GLuint textureUnit, Sampler *sampler);
    GLuint getSamplerId(GLuint textureUnit) const;

    Sampler *getSampler(GLuint textureUnit) const { return mSamplers[textureUnit].get(); }

    using SamplerBindingVector = std::vector<BindingPointer<Sampler>>;
    const SamplerBindingVector &getSamplers() const { return mSamplers; }

    void detachSampler(const Context *context, GLuint sampler);

    // Renderbuffer binding manipulation
    void setRenderbufferBinding(const Context *context, Renderbuffer *renderbuffer);
    GLuint getRenderbufferId() const;
    Renderbuffer *getCurrentRenderbuffer() const;
    void detachRenderbuffer(const Context *context, GLuint renderbuffer);

    // Framebuffer binding manipulation
    void setReadFramebufferBinding(Framebuffer *framebuffer);
    void setDrawFramebufferBinding(Framebuffer *framebuffer);
    Framebuffer *getTargetFramebuffer(GLenum target) const;
    Framebuffer *getReadFramebuffer() const;
    Framebuffer *getDrawFramebuffer() const { return mDrawFramebuffer; }

    bool removeReadFramebufferBinding(GLuint framebuffer);
    bool removeDrawFramebufferBinding(GLuint framebuffer);

    // Vertex array object binding manipulation
    void setVertexArrayBinding(const Context *context, VertexArray *vertexArray);
    GLuint getVertexArrayId() const;
    VertexArray *getVertexArray() const
    {
        ASSERT(mVertexArray != nullptr);
        return mVertexArray;
    }

    bool removeVertexArrayBinding(const Context *context, GLuint vertexArray);

    // Program binding manipulation
    angle::Result setProgram(const Context *context, Program *newProgram);

    Program *getProgram() const
    {
        ASSERT(!mProgram || !mProgram->isLinking());
        return mProgram;
    }

    Program *getLinkedProgram(const Context *context) const
    {
        if (mProgram)
        {
            mProgram->resolveLink(context);
        }
        return mProgram;
    }

    // Transform feedback object (not buffer) binding manipulation
    void setTransformFeedbackBinding(const Context *context, TransformFeedback *transformFeedback);
    TransformFeedback *getCurrentTransformFeedback() const { return mTransformFeedback.get(); }

    ANGLE_INLINE bool isTransformFeedbackActiveUnpaused() const
    {
        TransformFeedback *curTransformFeedback = mTransformFeedback.get();
        return curTransformFeedback && curTransformFeedback->isActive() &&
               !curTransformFeedback->isPaused();
    }

    bool removeTransformFeedbackBinding(const Context *context, GLuint transformFeedback);

    // Query binding manipulation
    bool isQueryActive(QueryType type) const;
    bool isQueryActive(Query *query) const;
    void setActiveQuery(const Context *context, QueryType type, Query *query);
    GLuint getActiveQueryId(QueryType type) const;
    Query *getActiveQuery(QueryType type) const;

    // Program Pipeline binding manipulation
    void setProgramPipelineBinding(const Context *context, ProgramPipeline *pipeline);
    void detachProgramPipeline(const Context *context, GLuint pipeline);

    //// Typed buffer binding point manipulation ////
    void setBufferBinding(const Context *context, BufferBinding target, Buffer *buffer);
    Buffer *getTargetBuffer(BufferBinding target) const;
    void setIndexedBufferBinding(const Context *context,
                                 BufferBinding target,
                                 GLuint index,
                                 Buffer *buffer,
                                 GLintptr offset,
                                 GLsizeiptr size);

    const OffsetBindingPointer<Buffer> &getIndexedUniformBuffer(size_t index) const;
    const OffsetBindingPointer<Buffer> &getIndexedAtomicCounterBuffer(size_t index) const;
    const OffsetBindingPointer<Buffer> &getIndexedShaderStorageBuffer(size_t index) const;

    // Detach a buffer from all bindings
    void detachBuffer(const Context *context, const Buffer *buffer);

    // Vertex attrib manipulation
    void setEnableVertexAttribArray(unsigned int attribNum, bool enabled);
    void setVertexAttribf(GLuint index, const GLfloat values[4]);
    void setVertexAttribu(GLuint index, const GLuint values[4]);
    void setVertexAttribi(GLuint index, const GLint values[4]);
    void setVertexAttribPointer(const Context *context,
                                unsigned int attribNum,
                                Buffer *boundBuffer,
                                GLint size,
                                GLenum type,
                                bool normalized,
                                bool pureInteger,
                                GLsizei stride,
                                const void *pointer);
    void setVertexAttribDivisor(const Context *context, GLuint index, GLuint divisor);
    const VertexAttribCurrentValueData &getVertexAttribCurrentValue(size_t attribNum) const;
    const std::vector<VertexAttribCurrentValueData> &getVertexAttribCurrentValues() const;
    const void *getVertexAttribPointer(unsigned int attribNum) const;
    void bindVertexBuffer(const Context *context,
                          GLuint bindingIndex,
                          Buffer *boundBuffer,
                          GLintptr offset,
                          GLsizei stride);
    void setVertexAttribFormat(GLuint attribIndex,
                               GLint size,
                               GLenum type,
                               bool normalized,
                               bool pureInteger,
                               GLuint relativeOffset);
    void setVertexAttribBinding(const Context *context, GLuint attribIndex, GLuint bindingIndex);
    void setVertexBindingDivisor(GLuint bindingIndex, GLuint divisor);

    // Pixel pack state manipulation
    void setPackAlignment(GLint alignment);
    GLint getPackAlignment() const;
    void setPackReverseRowOrder(bool reverseRowOrder);
    bool getPackReverseRowOrder() const;
    void setPackRowLength(GLint rowLength);
    GLint getPackRowLength() const;
    void setPackSkipRows(GLint skipRows);
    GLint getPackSkipRows() const;
    void setPackSkipPixels(GLint skipPixels);
    GLint getPackSkipPixels() const;
    const PixelPackState &getPackState() const;
    PixelPackState &getPackState();

    // Pixel unpack state manipulation
    void setUnpackAlignment(GLint alignment);
    GLint getUnpackAlignment() const;
    void setUnpackRowLength(GLint rowLength);
    GLint getUnpackRowLength() const;
    void setUnpackImageHeight(GLint imageHeight);
    GLint getUnpackImageHeight() const;
    void setUnpackSkipImages(GLint skipImages);
    GLint getUnpackSkipImages() const;
    void setUnpackSkipRows(GLint skipRows);
    GLint getUnpackSkipRows() const;
    void setUnpackSkipPixels(GLint skipPixels);
    GLint getUnpackSkipPixels() const;
    const PixelUnpackState &getUnpackState() const;
    PixelUnpackState &getUnpackState();

    // Debug state
    const Debug &getDebug() const;
    Debug &getDebug();

    // CHROMIUM_framebuffer_mixed_samples coverage modulation
    void setCoverageModulation(GLenum components);
    GLenum getCoverageModulation() const;

    // CHROMIUM_path_rendering
    void loadPathRenderingMatrix(GLenum matrixMode, const GLfloat *matrix);
    const GLfloat *getPathRenderingMatrix(GLenum which) const;
    void setPathStencilFunc(GLenum func, GLint ref, GLuint mask);

    GLenum getPathStencilFunc() const;
    GLint getPathStencilRef() const;
    GLuint getPathStencilMask() const;

    // GL_EXT_sRGB_write_control
    void setFramebufferSRGB(bool sRGB);
    bool getFramebufferSRGB() const;

    // GL_KHR_parallel_shader_compile
    void setMaxShaderCompilerThreads(GLuint count);
    GLuint getMaxShaderCompilerThreads() const;

    // State query functions
    void getBooleanv(GLenum pname, GLboolean *params);
    void getFloatv(GLenum pname, GLfloat *params);
    Error getIntegerv(const Context *context, GLenum pname, GLint *params);
    void getPointerv(const Context *context, GLenum pname, void **params) const;
    void getIntegeri_v(GLenum target, GLuint index, GLint *data);
    void getInteger64i_v(GLenum target, GLuint index, GLint64 *data);
    void getBooleani_v(GLenum target, GLuint index, GLboolean *data);

    bool isRobustResourceInitEnabled() const { return mRobustResourceInit; }

    // Sets the dirty bit for the program executable.
    angle::Result onProgramExecutableChange(const Context *context, Program *program);

    enum DirtyBitType
    {
        DIRTY_BIT_SCISSOR_TEST_ENABLED,
        DIRTY_BIT_SCISSOR,
        DIRTY_BIT_VIEWPORT,
        DIRTY_BIT_DEPTH_RANGE,
        DIRTY_BIT_BLEND_ENABLED,
        DIRTY_BIT_BLEND_COLOR,
        DIRTY_BIT_BLEND_FUNCS,
        DIRTY_BIT_BLEND_EQUATIONS,
        DIRTY_BIT_COLOR_MASK,
        DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED,
        DIRTY_BIT_SAMPLE_COVERAGE_ENABLED,
        DIRTY_BIT_SAMPLE_COVERAGE,
        DIRTY_BIT_SAMPLE_MASK_ENABLED,
        DIRTY_BIT_SAMPLE_MASK,
        DIRTY_BIT_DEPTH_TEST_ENABLED,
        DIRTY_BIT_DEPTH_FUNC,
        DIRTY_BIT_DEPTH_MASK,
        DIRTY_BIT_STENCIL_TEST_ENABLED,
        DIRTY_BIT_STENCIL_FUNCS_FRONT,
        DIRTY_BIT_STENCIL_FUNCS_BACK,
        DIRTY_BIT_STENCIL_OPS_FRONT,
        DIRTY_BIT_STENCIL_OPS_BACK,
        DIRTY_BIT_STENCIL_WRITEMASK_FRONT,
        DIRTY_BIT_STENCIL_WRITEMASK_BACK,
        DIRTY_BIT_CULL_FACE_ENABLED,
        DIRTY_BIT_CULL_FACE,
        DIRTY_BIT_FRONT_FACE,
        DIRTY_BIT_POLYGON_OFFSET_FILL_ENABLED,
        DIRTY_BIT_POLYGON_OFFSET,
        DIRTY_BIT_RASTERIZER_DISCARD_ENABLED,
        DIRTY_BIT_LINE_WIDTH,
        DIRTY_BIT_PRIMITIVE_RESTART_ENABLED,
        DIRTY_BIT_CLEAR_COLOR,
        DIRTY_BIT_CLEAR_DEPTH,
        DIRTY_BIT_CLEAR_STENCIL,
        DIRTY_BIT_UNPACK_STATE,
        DIRTY_BIT_UNPACK_BUFFER_BINDING,
        DIRTY_BIT_PACK_STATE,
        DIRTY_BIT_PACK_BUFFER_BINDING,
        DIRTY_BIT_DITHER_ENABLED,
        DIRTY_BIT_GENERATE_MIPMAP_HINT,
        DIRTY_BIT_SHADER_DERIVATIVE_HINT,
        DIRTY_BIT_READ_FRAMEBUFFER_BINDING,
        DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING,
        DIRTY_BIT_RENDERBUFFER_BINDING,
        DIRTY_BIT_VERTEX_ARRAY_BINDING,
        DIRTY_BIT_DRAW_INDIRECT_BUFFER_BINDING,
        DIRTY_BIT_DISPATCH_INDIRECT_BUFFER_BINDING,
        DIRTY_BIT_SHADER_STORAGE_BUFFER_BINDING,
        DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING,
        // TODO(jmadill): Fine-grained dirty bits for each index.
        DIRTY_BIT_UNIFORM_BUFFER_BINDINGS,
        DIRTY_BIT_PROGRAM_BINDING,
        DIRTY_BIT_PROGRAM_EXECUTABLE,
        // TODO(jmadill): Fine-grained dirty bits for each texture/sampler.
        DIRTY_BIT_TEXTURE_BINDINGS,
        DIRTY_BIT_SAMPLER_BINDINGS,
        DIRTY_BIT_IMAGE_BINDINGS,
        DIRTY_BIT_TRANSFORM_FEEDBACK_BINDING,
        DIRTY_BIT_MULTISAMPLING,
        DIRTY_BIT_SAMPLE_ALPHA_TO_ONE,
        DIRTY_BIT_COVERAGE_MODULATION,  // CHROMIUM_framebuffer_mixed_samples
        DIRTY_BIT_PATH_RENDERING,
        DIRTY_BIT_FRAMEBUFFER_SRGB,  // GL_EXT_sRGB_write_control
        DIRTY_BIT_CURRENT_VALUES,
        DIRTY_BIT_INVALID,
        DIRTY_BIT_MAX = DIRTY_BIT_INVALID,
    };

    static_assert(DIRTY_BIT_MAX <= 64, "State dirty bits must be capped at 64");

    // TODO(jmadill): Consider storing dirty objects in a list instead of by binding.
    enum DirtyObjectType
    {
        DIRTY_OBJECT_READ_FRAMEBUFFER,
        DIRTY_OBJECT_DRAW_FRAMEBUFFER,
        DIRTY_OBJECT_VERTEX_ARRAY,
        DIRTY_OBJECT_SAMPLERS,
        // Use a very coarse bit for any program or texture change.
        // TODO(jmadill): Fine-grained dirty bits for each texture/sampler.
        DIRTY_OBJECT_PROGRAM_TEXTURES,
        DIRTY_OBJECT_PROGRAM,
        DIRTY_OBJECT_UNKNOWN,
        DIRTY_OBJECT_MAX = DIRTY_OBJECT_UNKNOWN,
    };

    using DirtyBits = angle::BitSet<DIRTY_BIT_MAX>;
    const DirtyBits &getDirtyBits() const { return mDirtyBits; }
    void clearDirtyBits() { mDirtyBits.reset(); }
    void clearDirtyBits(const DirtyBits &bitset) { mDirtyBits &= ~bitset; }
    void setAllDirtyBits() { mDirtyBits.set(); }

    using DirtyObjects = angle::BitSet<DIRTY_OBJECT_MAX>;
    void clearDirtyObjects() { mDirtyObjects.reset(); }
    void setAllDirtyObjects() { mDirtyObjects.set(); }
    angle::Result syncDirtyObjects(const Context *context, const DirtyObjects &bitset);
    angle::Result syncDirtyObject(const Context *context, GLenum target);
    void setObjectDirty(GLenum target);
    void setSamplerDirty(size_t samplerIndex);

    // This actually clears the current value dirty bits.
    // TODO(jmadill): Pass mutable dirty bits into Impl.
    AttributesMask getAndResetDirtyCurrentValues() const;

    void setImageUnit(const Context *context,
                      size_t unit,
                      Texture *texture,
                      GLint level,
                      GLboolean layered,
                      GLint layer,
                      GLenum access,
                      GLenum format);

    const ImageUnit &getImageUnit(size_t unit) const;
    const ActiveTexturePointerArray &getActiveTexturesCache() const { return mActiveTexturesCache; }
    ComponentTypeMask getCurrentValuesTypeMask() const { return mCurrentValuesTypeMask; }

    void onActiveTextureStateChange(size_t textureIndex);
    void onUniformBufferStateChange(size_t uniformBufferIndex);

    angle::Result clearUnclearedActiveTextures(const Context *context);

    bool isCurrentTransformFeedback(const TransformFeedback *tf) const;

    bool isCurrentVertexArray(const VertexArray *va) const { return va == mVertexArray; }

    GLES1State &gles1() { return mGLES1State; }
    const GLES1State &gles1() const { return mGLES1State; }

  private:
    void syncSamplers(const Context *context);
    angle::Result syncProgramTextures(const Context *context);
    void unsetActiveTextures(ActiveTextureMask textureMask);
    angle::Result updateActiveTexture(const Context *context,
                                      size_t textureIndex,
                                      Texture *texture);

    // Cached values from Context's caps
    GLuint mMaxDrawBuffers;
    GLuint mMaxCombinedTextureImageUnits;

    ColorF mColorClearValue;
    GLfloat mDepthClearValue;
    int mStencilClearValue;

    RasterizerState mRasterizer;
    bool mScissorTest;
    Rectangle mScissor;

    BlendState mBlend;
    ColorF mBlendColor;
    bool mSampleCoverage;
    GLfloat mSampleCoverageValue;
    bool mSampleCoverageInvert;
    bool mSampleMask;
    GLuint mMaxSampleMaskWords;
    std::array<GLbitfield, MAX_SAMPLE_MASK_WORDS> mSampleMaskValues;

    DepthStencilState mDepthStencil;
    GLint mStencilRef;
    GLint mStencilBackRef;

    GLfloat mLineWidth;

    GLenum mGenerateMipmapHint;
    GLenum mFragmentShaderDerivativeHint;

    const bool mBindGeneratesResource;
    const bool mClientArraysEnabled;

    Rectangle mViewport;
    float mNearZ;
    float mFarZ;

    Framebuffer *mReadFramebuffer;
    Framebuffer *mDrawFramebuffer;
    BindingPointer<Renderbuffer> mRenderbuffer;
    Program *mProgram;
    BindingPointer<ProgramPipeline> mProgramPipeline;

    using VertexAttribVector = std::vector<VertexAttribCurrentValueData>;
    VertexAttribVector mVertexAttribCurrentValues;  // From glVertexAttrib
    VertexArray *mVertexArray;
    ComponentTypeMask mCurrentValuesTypeMask;

    // Texture and sampler bindings
    size_t mActiveSampler;  // Active texture unit selector - GL_TEXTURE0

    using TextureBindingVector = std::vector<BindingPointer<Texture>>;
    using TextureBindingMap    = angle::PackedEnumMap<TextureType, TextureBindingVector>;
    TextureBindingMap mSamplerTextures;

    // Texture Completeness Caching
    // ----------------------------
    // The texture completeness cache uses dirty bits to avoid having to scan the list of textures
    // each draw call. This gl::State class implements angle::Observer interface. When subject
    // Textures have state changes, messages reach 'State' (also any observing Framebuffers) via the
    // onSubjectStateChange method (above). This then invalidates the completeness cache.
    //
    // Note this requires that we also invalidate the completeness cache manually on events like
    // re-binding textures/samplers or a change in the program. For more information see the
    // Observer.h header and the design doc linked there.

    // A cache of complete textures. nullptr indicates unbound or incomplete.
    // Don't use BindingPointer because this cache is only valid within a draw call.
    // Also stores a notification channel to the texture itself to handle texture change events.
    ActiveTexturePointerArray mActiveTexturesCache;
    std::vector<angle::ObserverBinding> mCompleteTextureBindings;
    InitState mCachedTexturesInitState;

    InitState mCachedImageTexturesInitState;

    SamplerBindingVector mSamplers;

    using ImageUnitVector = std::vector<ImageUnit>;
    ImageUnitVector mImageUnits;

    using ActiveQueryMap = angle::PackedEnumMap<QueryType, BindingPointer<Query>>;
    ActiveQueryMap mActiveQueries;

    // Stores the currently bound buffer for each binding point. It has an entry for the element
    // array buffer but it should not be used. Instead this bind point is owned by the current
    // vertex array object.
    using BoundBufferMap = angle::PackedEnumMap<BufferBinding, BindingPointer<Buffer>>;
    BoundBufferMap mBoundBuffers;

    using BufferVector = std::vector<OffsetBindingPointer<Buffer>>;
    BufferVector mUniformBuffers;
    BufferVector mAtomicCounterBuffers;
    BufferVector mShaderStorageBuffers;

    BindingPointer<TransformFeedback> mTransformFeedback;

    PixelUnpackState mUnpack;
    PixelPackState mPack;

    bool mPrimitiveRestart;

    Debug mDebug;

    bool mMultiSampling;
    bool mSampleAlphaToOne;

    GLenum mCoverageModulation;

    // CHROMIUM_path_rendering
    GLfloat mPathMatrixMV[16];
    GLfloat mPathMatrixProj[16];
    GLenum mPathStencilFunc;
    GLint mPathStencilRef;
    GLuint mPathStencilMask;

    // GL_EXT_sRGB_write_control
    bool mFramebufferSRGB;

    // GL_ANGLE_robust_resource_intialization
    const bool mRobustResourceInit;

    // GL_ANGLE_program_cache_control
    const bool mProgramBinaryCacheEnabled;

    // GL_KHR_parallel_shader_compile
    GLuint mMaxShaderCompilerThreads;

    // GLES1 emulation: state specific to GLES1
    GLES1State mGLES1State;

    DirtyBits mDirtyBits;
    DirtyObjects mDirtyObjects;
    mutable AttributesMask mDirtyCurrentValues;
    ActiveTextureMask mDirtySamplers;
};

}  // namespace gl

#endif  // LIBANGLE_STATE_H_
