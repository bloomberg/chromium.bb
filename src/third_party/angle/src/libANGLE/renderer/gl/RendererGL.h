//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererGL.h: Defines the class interface for RendererGL.

#ifndef LIBANGLE_RENDERER_GL_RENDERERGL_H_
#define LIBANGLE_RENDERER_GL_RENDERERGL_H_

#include "libANGLE/Caps.h"
#include "libANGLE/Error.h"
#include "libANGLE/Version.h"
#include "libANGLE/renderer/gl/WorkaroundsGL.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

namespace gl
{
class ContextState;
struct IndexRange;
class Path;
struct Workarounds;
}  // namespace gl

namespace egl
{
class AttributeMap;
}

namespace sh
{
struct BlockMemberInfo;
}

namespace rx
{
class BlitGL;
class ClearMultiviewGL;
class ContextImpl;
class FunctionsGL;
class StateManagerGL;

class RendererGL : angle::NonCopyable
{
  public:
    RendererGL(std::unique_ptr<FunctionsGL> functions, const egl::AttributeMap &attribMap);
    virtual ~RendererGL();

    angle::Result flush();
    angle::Result finish();

    // CHROMIUM_path_rendering implementation
    void stencilFillPath(const gl::ContextState &state,
                         const gl::Path *path,
                         GLenum fillMode,
                         GLuint mask);
    void stencilStrokePath(const gl::ContextState &state,
                           const gl::Path *path,
                           GLint reference,
                           GLuint mask);
    void coverFillPath(const gl::ContextState &state, const gl::Path *path, GLenum coverMode);
    void coverStrokePath(const gl::ContextState &state, const gl::Path *path, GLenum coverMode);
    void stencilThenCoverFillPath(const gl::ContextState &state,
                                  const gl::Path *path,
                                  GLenum fillMode,
                                  GLuint mask,
                                  GLenum coverMode);
    void stencilThenCoverStrokePath(const gl::ContextState &state,
                                    const gl::Path *path,
                                    GLint reference,
                                    GLuint mask,
                                    GLenum coverMode);
    void coverFillPathInstanced(const gl::ContextState &state,
                                const std::vector<gl::Path *> &paths,
                                GLenum coverMode,
                                GLenum transformType,
                                const GLfloat *transformValues);
    void coverStrokePathInstanced(const gl::ContextState &state,
                                  const std::vector<gl::Path *> &paths,
                                  GLenum coverMode,
                                  GLenum transformType,
                                  const GLfloat *transformValues);
    void stencilFillPathInstanced(const gl::ContextState &state,
                                  const std::vector<gl::Path *> &paths,
                                  GLenum fillMode,
                                  GLuint mask,
                                  GLenum transformType,
                                  const GLfloat *transformValues);
    void stencilStrokePathInstanced(const gl::ContextState &state,
                                    const std::vector<gl::Path *> &paths,
                                    GLint reference,
                                    GLuint mask,
                                    GLenum transformType,
                                    const GLfloat *transformValues);

    void stencilThenCoverFillPathInstanced(const gl::ContextState &state,
                                           const std::vector<gl::Path *> &paths,
                                           GLenum coverMode,
                                           GLenum fillMode,
                                           GLuint mask,
                                           GLenum transformType,
                                           const GLfloat *transformValues);
    void stencilThenCoverStrokePathInstanced(const gl::ContextState &state,
                                             const std::vector<gl::Path *> &paths,
                                             GLenum coverMode,
                                             GLint reference,
                                             GLuint mask,
                                             GLenum transformType,
                                             const GLfloat *transformValues);

    GLenum getResetStatus();

    // EXT_debug_marker
    void insertEventMarker(GLsizei length, const char *marker);
    void pushGroupMarker(GLsizei length, const char *marker);
    void popGroupMarker();

    // KHR_debug
    void pushDebugGroup(GLenum source, GLuint id, GLsizei length, const char *message);
    void popDebugGroup();

    std::string getVendorString() const;
    std::string getRendererDescription() const;

    GLint getGPUDisjoint();
    GLint64 getTimestamp();

    const gl::Version &getMaxSupportedESVersion() const;
    const FunctionsGL *getFunctions() const { return mFunctions.get(); }
    StateManagerGL *getStateManager() const { return mStateManager; }
    const WorkaroundsGL &getWorkarounds() const { return mWorkarounds; }
    BlitGL *getBlitter() const { return mBlitter; }
    ClearMultiviewGL *getMultiviewClearer() const { return mMultiviewClearer; }

    MultiviewImplementationTypeGL getMultiviewImplementationType() const;
    const gl::Caps &getNativeCaps() const;
    const gl::TextureCapsMap &getNativeTextureCaps() const;
    const gl::Extensions &getNativeExtensions() const;
    const gl::Limitations &getNativeLimitations() const;
    void applyNativeWorkarounds(gl::Workarounds *workarounds) const;

    angle::Result dispatchCompute(const gl::Context *context,
                                  GLuint numGroupsX,
                                  GLuint numGroupsY,
                                  GLuint numGroupsZ);
    angle::Result dispatchComputeIndirect(const gl::Context *context, GLintptr indirect);

    angle::Result memoryBarrier(GLbitfield barriers);
    angle::Result memoryBarrierByRegion(GLbitfield barriers);

  private:
    void ensureCapsInitialized() const;
    void generateCaps(gl::Caps *outCaps,
                      gl::TextureCapsMap *outTextureCaps,
                      gl::Extensions *outExtensions,
                      gl::Limitations *outLimitations) const;

    mutable gl::Version mMaxSupportedESVersion;

    std::unique_ptr<FunctionsGL> mFunctions;
    StateManagerGL *mStateManager;

    BlitGL *mBlitter;
    ClearMultiviewGL *mMultiviewClearer;

    WorkaroundsGL mWorkarounds;

    bool mUseDebugOutput;

    mutable bool mCapsInitialized;
    mutable gl::Caps mNativeCaps;
    mutable gl::TextureCapsMap mNativeTextureCaps;
    mutable gl::Extensions mNativeExtensions;
    mutable gl::Limitations mNativeLimitations;
    mutable MultiviewImplementationTypeGL mMultiviewImplementationType;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_RENDERERGL_H_
