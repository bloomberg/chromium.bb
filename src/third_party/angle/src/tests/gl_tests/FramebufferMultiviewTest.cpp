//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Framebuffer multiview tests:
// The tests modify and examine the multiview state.
//

#include "test_utils/MultiviewTest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{
std::vector<GLenum> GetDrawBufferRange(size_t numColorAttachments)
{
    std::vector<GLenum> drawBuffers(numColorAttachments);
    const size_t kBase = static_cast<size_t>(GL_COLOR_ATTACHMENT0);
    for (size_t i = 0u; i < drawBuffers.size(); ++i)
    {
        drawBuffers[i] = static_cast<GLenum>(kBase + i);
    }
    return drawBuffers;
}
}  // namespace

// Base class for tests that care mostly about draw call validity and not rendering results.
class FramebufferMultiviewTest : public MultiviewTest
{
  protected:
    FramebufferMultiviewTest() : MultiviewTest() {}
};

class FramebufferMultiviewSideBySideClearTest : public FramebufferMultiviewTest
{
  protected:
    FramebufferMultiviewSideBySideClearTest()
        : mMultiviewFBO(0u), mNonMultiviewFBO(0u), mDepthTex(0u), mDepthStencilTex(0u)
    {
    }

    void TearDown() override
    {
        if (mMultiviewFBO != 0)
        {
            glDeleteFramebuffers(1, &mMultiviewFBO);
            mMultiviewFBO = 0u;
        }
        if (mNonMultiviewFBO != 0)
        {
            glDeleteTextures(1, &mNonMultiviewFBO);
            mNonMultiviewFBO = 0u;
        }
        if (!mColorTex.empty())
        {
            GLsizei textureCount = static_cast<GLsizei>(mColorTex.size());
            glDeleteTextures(textureCount, mColorTex.data());
            mColorTex.clear();
        }
        if (mDepthStencilTex != 0u)
        {
            glDeleteTextures(1, &mDepthStencilTex);
            mDepthStencilTex = 0u;
        }
        if (mDepthTex != 0u)
        {
            glDeleteTextures(1, &mDepthTex);
            mDepthTex = 0u;
        }
        MultiviewTest::TearDown();
    }

    void initializeFBOs(size_t numColorBuffers, bool stencil, bool depth)
    {
        ASSERT(mColorTex.empty() && mDepthStencilTex == 0u && mDepthTex == 0u);
        const std::vector<GLenum> &drawBuffers = GetDrawBufferRange(numColorBuffers);

        // Generate textures.
        mColorTex.resize(numColorBuffers);
        GLsizei textureCount = static_cast<GLsizei>(mColorTex.size());
        glGenTextures(textureCount, mColorTex.data());
        if (stencil)
        {
            glGenTextures(1, &mDepthStencilTex);
        }
        else if (depth)
        {
            glGenTextures(1, &mDepthTex);
        }

        CreateMultiviewBackingTextures(GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE, 2, 2, 2,
                                       mColorTex, mDepthTex, mDepthStencilTex);

        glGenFramebuffers(1, &mMultiviewFBO);
        glGenFramebuffers(1, &mNonMultiviewFBO);

        // Generate multiview fbo and attach textures.
        const GLint kViewportOffsets[4] = {1, 0, 3, 0};
        glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);
        const size_t kBase = static_cast<size_t>(GL_COLOR_ATTACHMENT0);
        for (size_t i = 0u; i < numColorBuffers; ++i)
        {
            glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER,
                                                         static_cast<GLenum>(kBase + i),
                                                         mColorTex[i], 0, 2, &kViewportOffsets[0]);
        }

        if (stencil)
        {
            glFramebufferTextureMultiviewSideBySideANGLE(
                GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, mDepthStencilTex, 0, 2,
                &kViewportOffsets[0]);
        }
        else if (depth)
        {
            glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                                         mDepthTex, 0, 2, &kViewportOffsets[0]);
        }
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());

        // Generate non-multiview fbo and attach textures.
        glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO);
        for (size_t i = 0u; i < numColorBuffers; ++i)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, static_cast<GLenum>(kBase + i), GL_TEXTURE_2D,
                                   mColorTex[i], 0);
        }
        if (stencil)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                                   mDepthStencilTex, 0);
        }
        else if (depth)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mDepthTex,
                                   0);
        }
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());

        ASSERT_GL_NO_ERROR();
    }

    void checkAlternatingColumnsOfRedAndGreenInFBO()
    {
        // column 0
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(0, 1, GLColor::red);

        // column 1
        EXPECT_PIXEL_COLOR_EQ(1, 0, GLColor::green);
        EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::green);

        // column 2
        EXPECT_PIXEL_COLOR_EQ(2, 0, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(2, 1, GLColor::red);

        // column 3
        EXPECT_PIXEL_COLOR_EQ(3, 0, GLColor::green);
        EXPECT_PIXEL_COLOR_EQ(3, 1, GLColor::green);
    }

    GLuint mMultiviewFBO;
    GLuint mNonMultiviewFBO;

  private:
    std::vector<GLuint> mColorTex;
    GLuint mDepthTex;
    GLuint mDepthStencilTex;
};

class FramebufferMultiviewLayeredClearTest : public FramebufferMultiviewTest
{
  protected:
    FramebufferMultiviewLayeredClearTest() : mMultiviewFBO(0), mDepthTex(0), mDepthStencilTex(0) {}

    void TearDown() override
    {
        if (mMultiviewFBO != 0)
        {
            glDeleteFramebuffers(1, &mMultiviewFBO);
            mMultiviewFBO = 0u;
        }
        if (!mNonMultiviewFBO.empty())
        {
            GLsizei textureCount = static_cast<GLsizei>(mNonMultiviewFBO.size());
            glDeleteTextures(textureCount, mNonMultiviewFBO.data());
            mNonMultiviewFBO.clear();
        }
        if (!mColorTex.empty())
        {
            GLsizei textureCount = static_cast<GLsizei>(mColorTex.size());
            glDeleteTextures(textureCount, mColorTex.data());
            mColorTex.clear();
        }
        if (mDepthStencilTex != 0u)
        {
            glDeleteTextures(1, &mDepthStencilTex);
            mDepthStencilTex = 0u;
        }
        if (mDepthTex != 0u)
        {
            glDeleteTextures(1, &mDepthTex);
            mDepthTex = 0u;
        }
        MultiviewTest::TearDown();
    }

    void initializeFBOs(int width,
                        int height,
                        int numLayers,
                        int baseViewIndex,
                        int numViews,
                        int numColorAttachments,
                        bool stencil,
                        bool depth)
    {
        ASSERT(mColorTex.empty() && mDepthStencilTex == 0u && mDepthTex == 0u);
        ASSERT(baseViewIndex + numViews <= numLayers);

        // Generate textures.
        mColorTex.resize(numColorAttachments);
        GLsizei textureCount = static_cast<GLsizei>(mColorTex.size());
        glGenTextures(textureCount, mColorTex.data());
        if (stencil)
        {
            glGenTextures(1, &mDepthStencilTex);
        }
        else if (depth)
        {
            glGenTextures(1, &mDepthTex);
        }

        CreateMultiviewBackingTextures(GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE, width, height,
                                       numLayers, mColorTex, mDepthTex, mDepthStencilTex);

        glGenFramebuffers(1, &mMultiviewFBO);

        // Generate multiview FBO and attach textures.
        glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);
        AttachMultiviewTextures(GL_FRAMEBUFFER, GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE, width,
                                numViews, baseViewIndex, mColorTex, mDepthTex, mDepthStencilTex);

        const auto &drawBuffers = GetDrawBufferRange(numColorAttachments);
        glDrawBuffers(numColorAttachments, drawBuffers.data());

        // Generate non-multiview FBOs and attach textures.
        mNonMultiviewFBO.resize(numLayers);
        GLsizei framebufferCount = static_cast<GLsizei>(mNonMultiviewFBO.size());
        glGenFramebuffers(framebufferCount, mNonMultiviewFBO.data());
        for (int i = 0; i < numLayers; ++i)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO[i]);
            for (int j = 0; j < numColorAttachments; ++j)
            {
                glFramebufferTextureLayer(GL_FRAMEBUFFER,
                                          static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + j),
                                          mColorTex[j], 0, i);
            }
            if (stencil)
            {
                glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                          mDepthStencilTex, 0, i);
            }
            else if (depth)
            {
                glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, mDepthTex, 0, i);
            }
            glDrawBuffers(numColorAttachments, drawBuffers.data());
        }

        ASSERT_GL_NO_ERROR();
    }

    GLColor getLayerColor(size_t layer, GLenum attachment, GLint x, GLint y)
    {
        ASSERT(layer < mNonMultiviewFBO.size());
        glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO[layer]);
        glReadBuffer(attachment);
        return angle::ReadColor(x, y);
    }

    GLColor getLayerColor(size_t layer, GLenum attachment)
    {
        return getLayerColor(layer, attachment, 0, 0);
    }

    GLuint mMultiviewFBO;
    std::vector<GLuint> mNonMultiviewFBO;

  private:
    std::vector<GLuint> mColorTex;
    GLuint mDepthTex;
    GLuint mDepthStencilTex;
};

// Test that the framebuffer tokens introduced by ANGLE_multiview can be used to query the
// framebuffer state and that their corresponding default values are correctly set.
TEST_P(FramebufferMultiviewTest, DefaultState)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    GLint numViews = -1;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_ANGLE,
                                          &numViews);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, numViews);

    GLint baseViewIndex = -1;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_ANGLE,
                                          &baseViewIndex);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(0, baseViewIndex);

    GLint multiviewLayout = GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_MULTIVIEW_LAYOUT_ANGLE,
                                          &multiviewLayout);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(GL_NONE, multiviewLayout);

    GLint viewportOffsets[2] = {-1, -1};
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_VIEWPORT_OFFSETS_ANGLE,
                                          &viewportOffsets[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(0, viewportOffsets[0]);
    EXPECT_EQ(0, viewportOffsets[1]);
}

// Test that without having the ANGLE_multiview extension, querying for the framebuffer state using
// the ANGLE_multiview tokens results in an INVALID_ENUM error.
TEST_P(FramebufferMultiviewTest, NegativeFramebufferStateQueries)
{
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    GLint numViews = -1;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_ANGLE,
                                          &numViews);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    GLint baseViewIndex = -1;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_ANGLE,
                                          &baseViewIndex);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    GLint multiviewLayout = GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_MULTIVIEW_LAYOUT_ANGLE,
                                          &multiviewLayout);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    GLint viewportOffsets[2] = {-1, -1};
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_VIEWPORT_OFFSETS_ANGLE,
                                          &viewportOffsets[0]);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Test that the correct errors are generated whenever glFramebufferTextureMultiviewSideBySideANGLE
// is called with invalid arguments.
TEST_P(FramebufferMultiviewTest, InvalidMultiviewSideBySideArguments)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Negative offsets.
    GLint viewportOffsets[2] = {-1};
    glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 1,
                                                 &viewportOffsets[0]);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Negative number of views.
    viewportOffsets[0] = 0;
    viewportOffsets[1] = 0;
    glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, -1,
                                                 &viewportOffsets[0]);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that the correct errors are generated whenever glFramebufferTextureMultiviewLayeredANGLE is
// called with invalid arguments.
TEST_P(FramebufferMultiviewTest, InvalidMultiviewLayeredArguments)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    // Negative base view index.
    glFramebufferTextureMultiviewLayeredANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, -1, 1);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // baseViewIndex + numViews is greater than MAX_TEXTURE_LAYERS.
    GLint maxTextureLayers = 0;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxTextureLayers);
    glFramebufferTextureMultiviewLayeredANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0,
                                              maxTextureLayers, 1);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that an INVALID_OPERATION error is generated whenever the ANGLE_multiview extension is not
// available.
TEST_P(FramebufferMultiviewTest, ExtensionNotAvailableCheck)
{
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    ASSERT_GL_NO_ERROR();
    const GLint kViewportOffsets[2] = {0};
    glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 1,
                                                 &kViewportOffsets[0]);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that glFramebufferTextureMultiviewSideBySideANGLE modifies the internal multiview state.
TEST_P(FramebufferMultiviewTest, ModifySideBySideState)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    const GLint kViewportOffsets[4] = {0, 0, 1, 2};
    glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 2,
                                                 &kViewportOffsets[0]);
    ASSERT_GL_NO_ERROR();

    GLint numViews = -1;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_ANGLE,
                                          &numViews);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(2, numViews);

    GLint baseViewIndex = -1;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_ANGLE,
                                          &baseViewIndex);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(0, baseViewIndex);

    GLint multiviewLayout = GL_NONE;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_MULTIVIEW_LAYOUT_ANGLE,
                                          &multiviewLayout);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE, multiviewLayout);

    GLint internalViewportOffsets[4] = {-1};
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_VIEWPORT_OFFSETS_ANGLE,
                                          &internalViewportOffsets[0]);
    ASSERT_GL_NO_ERROR();
    for (size_t i = 0u; i < 4u; ++i)
    {
        EXPECT_EQ(kViewportOffsets[i], internalViewportOffsets[i]);
    }
}

// Test framebuffer completeness status of a side-by-side framebuffer with color and depth
// attachments.
TEST_P(FramebufferMultiviewTest, IncompleteViewTargetsSideBySide)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    const GLint kViewportOffsets[4]      = {0, 0, 2, 0};
    const GLint kOtherViewportOffsets[4] = {2, 0, 4, 0};

    // Set the 0th attachment and keep it as it is till the end of the test. The 1st or depth
    // attachment will be modified to change the framebuffer's status.
    glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 2,
                                                 &kViewportOffsets[0]);
    ASSERT_GL_NO_ERROR();

    // Color attachment 1.
    {
        GLTexture otherTex;
        glBindTexture(GL_TEXTURE_2D, otherTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        // Test framebuffer completeness when the number of views differ.
        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, otherTex,
                                                     0, 1, &kViewportOffsets[0]);
        ASSERT_GL_NO_ERROR();
        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_ANGLE,
                         glCheckFramebufferStatus(GL_FRAMEBUFFER));

        // Test framebuffer completeness when the viewport offsets differ.
        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, otherTex,
                                                     0, 2, &kOtherViewportOffsets[0]);
        ASSERT_GL_NO_ERROR();
        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_ANGLE,
                         glCheckFramebufferStatus(GL_FRAMEBUFFER));

        // Test framebuffer completeness when attachment layouts differ.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, otherTex, 0);
        ASSERT_GL_NO_ERROR();
        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_ANGLE,
                         glCheckFramebufferStatus(GL_FRAMEBUFFER));

        // Test that framebuffer is complete when the number of views, viewport offsets and layouts
        // are the same.
        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, otherTex,
                                                     0, 2, &kViewportOffsets[0]);
        ASSERT_GL_NO_ERROR();
        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

        // Reset attachment 1
        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, 0, 0, 1,
                                                     &kViewportOffsets[0]);
    }

    // Depth attachment.
    {
        GLTexture depthTex;
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
                     nullptr);

        // Test framebuffer completeness when the number of views differ.
        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTex,
                                                     0, 1, &kViewportOffsets[0]);
        ASSERT_GL_NO_ERROR();
        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_ANGLE,
                         glCheckFramebufferStatus(GL_FRAMEBUFFER));

        // Test framebuffer completeness when the viewport offsets differ.
        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTex,
                                                     0, 2, &kOtherViewportOffsets[0]);
        ASSERT_GL_NO_ERROR();
        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_ANGLE,
                         glCheckFramebufferStatus(GL_FRAMEBUFFER));

        // Test framebuffer completeness when attachment layouts differ.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
        ASSERT_GL_NO_ERROR();
        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_ANGLE,
                         glCheckFramebufferStatus(GL_FRAMEBUFFER));

        // Test that framebuffer is complete when the number of views, viewport offsets and layouts
        // are the same.
        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTex,
                                                     0, 2, &kViewportOffsets[0]);
        ASSERT_GL_NO_ERROR();
        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    }
}

// Test that the active read framebuffer cannot be read from through glCopyTex* if it has multi-view
// attachments with a side-by-side layout.
TEST_P(FramebufferMultiviewTest, InvalidCopyTex)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    const GLint kViewportOffsets[2] = {0};
    glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 1,
                                                 &kViewportOffsets[0]);
    ASSERT_GL_NO_ERROR();

    // Test glCopyTexImage2D and glCopyTexSubImage2D.
    {
        GLTexture tex2;
        glBindTexture(GL_TEXTURE_2D, tex2);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 1, 1, 0);
        EXPECT_GL_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);

        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
        EXPECT_GL_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    }

    // Test glCopyTexSubImage3D.
    {
        GLTexture tex2;
        glBindTexture(GL_TEXTURE_3D, tex2);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 1, 1);
        EXPECT_GL_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    }
}

// Test that the active read framebuffer can be read with glCopyTex* if it only has one layered
// view.
TEST_P(FramebufferMultiviewTest, CopyTex)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glFramebufferTextureMultiviewLayeredANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 0, 1);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    ASSERT_GL_NO_ERROR();

    // Test glCopyTexImage2D and glCopyTexSubImage2D.
    {
        GLTexture tex2;
        glBindTexture(GL_TEXTURE_2D, tex2);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 1, 1, 0);
        ASSERT_GL_NO_ERROR();

        // Test texture contents.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        draw2DTexturedQuad(0.0f, 1.0f, true);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
        ASSERT_GL_NO_ERROR();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        draw2DTexturedQuad(0.0f, 1.0f, true);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::cyan);
    }

    // Test glCopyTexSubImage3D.
    {
        GLTexture tex2;
        glBindTexture(GL_TEXTURE_3D, tex2);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 1, 1);
        ASSERT_GL_NO_ERROR();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        draw3DTexturedQuad(0.0f, 1.0f, true, 0.0f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::yellow);
    }
}

// Test that glBlitFramebuffer generates an invalid framebuffer operation when either the current
// draw framebuffer, or current read framebuffer have multiview attachments with a side-by-side
// layout.
TEST_P(FramebufferMultiviewTest, InvalidBlit)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    const GLint kViewportOffsets[2] = {0};
    glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 1,
                                                 &kViewportOffsets[0]);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    ASSERT_GL_NO_ERROR();

    // Blit with the active read framebuffer having multiview attachments.
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        EXPECT_GL_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    }

    // Blit with the active draw framebuffer having multiview attachments.
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        glBlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        EXPECT_GL_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    }
}

// Test that glBlitFramebuffer succeeds if the current read framebuffer has just one layered view.
TEST_P(FramebufferMultiviewTest, Blit)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glFramebufferTextureMultiviewLayeredANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 0, 1);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    ASSERT_GL_NO_ERROR();

    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that glReadPixels generates an invalid framebuffer operation error if the current read
// framebuffer has a side-by-side multi-view layout.
TEST_P(FramebufferMultiviewTest, InvalidReadPixels)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    const GLint kViewportOffsets[2] = {0};
    glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 1,
                                                 &kViewportOffsets[0]);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    ASSERT_GL_NO_ERROR();

    GLColor pixelColor;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixelColor.R);
    EXPECT_GL_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
}

// Test that glReadPixels succeeds from a layered multiview framebuffer with just one view.
TEST_P(FramebufferMultiviewTest, ReadPixels)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glFramebufferTextureMultiviewLayeredANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 0, 1);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    ASSERT_GL_NO_ERROR();

    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLColor pixelColor;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixelColor.R);
    ASSERT_GL_NO_ERROR();
    EXPECT_COLOR_NEAR(GLColor::green, pixelColor, 2);
}

// Test that glClear clears only the contents of each view if the scissor test is enabled.
TEST_P(FramebufferMultiviewSideBySideClearTest, ColorBufferClear)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    initializeFBOs(1, false, false);

    // Clear the contents of the texture.
    glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Bind and specify viewport/scissor dimensions for each view.
    glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);
    glViewport(0, 0, 1, 2);
    glScissor(0, 0, 1, 2);
    glEnable(GL_SCISSOR_TEST);

    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO);

    checkAlternatingColumnsOfRedAndGreenInFBO();
}

// Test that glFramebufferTextureMultiviewLayeredANGLE modifies the internal multiview state.
TEST_P(FramebufferMultiviewTest, ModifyLayeredState)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    GLFramebuffer multiviewFBO;
    glBindFramebuffer(GL_FRAMEBUFFER, multiviewFBO);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    glFramebufferTextureMultiviewLayeredANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 1, 2);
    ASSERT_GL_NO_ERROR();

    GLint numViews = -1;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_ANGLE,
                                          &numViews);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(2, numViews);

    GLint baseViewIndex = -1;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_ANGLE,
                                          &baseViewIndex);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, baseViewIndex);

    GLint multiviewLayout = GL_NONE;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_MULTIVIEW_LAYOUT_ANGLE,
                                          &multiviewLayout);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE, multiviewLayout);

    GLint internalViewportOffsets[2] = {-1};
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_VIEWPORT_OFFSETS_ANGLE,
                                          &internalViewportOffsets[0]);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(0, internalViewportOffsets[0]);
    EXPECT_EQ(0, internalViewportOffsets[1]);
}

// Test framebuffer completeness status of a layered framebuffer with color attachments.
TEST_P(FramebufferMultiviewTest, IncompleteViewTargetsLayered)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Set the 0th attachment and keep it as it is till the end of the test. The 1st color
    // attachment will be modified to change the framebuffer's status.
    glFramebufferTextureMultiviewLayeredANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 0, 2);
    ASSERT_GL_NO_ERROR();

    GLTexture otherTexLayered;
    glBindTexture(GL_TEXTURE_2D_ARRAY, otherTexLayered);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Test framebuffer completeness when the base view index differs.
    glFramebufferTextureMultiviewLayeredANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, otherTexLayered,
                                              0, 1, 2);
    ASSERT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_ANGLE,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Test framebuffer completeness when the 1st attachment has a side-by-side layout.
    const int kViewportOffsets[4] = {0, 0, 0, 0};
    GLTexture otherTex2D;
    glBindTexture(GL_TEXTURE_2D, otherTex2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, otherTex2D,
                                                 0, 2, kViewportOffsets);
    ASSERT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_ANGLE,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Test framebuffer completeness when the 1st attachment has a non-multiview layout.
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, otherTexLayered, 0, 0);
    ASSERT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_ANGLE,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Test that framebuffer is complete when the number of views, base view index and layouts are
    // the same.
    glFramebufferTextureMultiviewLayeredANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, otherTexLayered,
                                              0, 0, 2);
    ASSERT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
}

// Test that glClear clears all of the contents if the scissor test is disabled.
TEST_P(FramebufferMultiviewSideBySideClearTest, ClearWithDisabledScissorTest)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    initializeFBOs(1, false, false);

    // Clear the contents of the texture.
    glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Bind and specify viewport/scissor dimensions for each view.
    glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);
    glViewport(0, 0, 1, 2);
    glScissor(0, 0, 1, 2);

    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO);

    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            EXPECT_PIXEL_COLOR_EQ(i, j, GLColor::red);
        }
    }
}

// Test that glClear clears the depth buffer of each view.
TEST_P(FramebufferMultiviewSideBySideClearTest, DepthBufferClear)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    // Create program to draw a quad.
    const std::string &vs =
        "#version 300 es\n"
        "in vec3 vPos;\n"
        "void main(){\n"
        "   gl_Position = vec4(vPos, 1.);\n"
        "}\n";
    const std::string &fs =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform vec3 uCol;\n"
        "out vec4 col;\n"
        "void main(){\n"
        "   col = vec4(uCol,1.);\n"
        "}\n";
    ANGLE_GL_PROGRAM(program, vs, fs);
    glUseProgram(program);
    GLuint mColorUniformLoc = glGetUniformLocation(program, "uCol");

    initializeFBOs(1, false, true);
    glEnable(GL_DEPTH_TEST);

    // Clear the contents of the texture.
    glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Dirty the depth and color buffers.
    glUniform3f(mColorUniformLoc, 1.0f, 0.0f, 0.0f);
    drawQuad(program, "vPos", 0.0f, 1.0f, true);

    // Switch to the multi-view framebuffer and clear only the rectangles covered by the views.
    glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);
    glViewport(0, 0, 1, 2);
    glScissor(0, 0, 1, 2);
    glEnable(GL_SCISSOR_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Draw a fullscreen quad to fill the cleared regions.
    glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO);
    glViewport(0, 0, 4, 2);
    glScissor(0, 0, 4, 2);
    glUniform3f(mColorUniformLoc, 0.0f, 1.0f, 0.0f);
    drawQuad(program, "vPos", 0.5f, 1.0f, true);

    checkAlternatingColumnsOfRedAndGreenInFBO();
}

// Test that glClear clears the stencil buffer of each view.
TEST_P(FramebufferMultiviewSideBySideClearTest, StencilBufferClear)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    // Create program to draw a quad.
    const std::string &vs =
        "#version 300 es\n"
        "in vec3 vPos;\n"
        "void main(){\n"
        "   gl_Position = vec4(vPos, 1.);\n"
        "}\n";
    const std::string &fs =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform vec3 uCol;\n"
        "out vec4 col;\n"
        "void main(){\n"
        "   col = vec4(uCol,1.);\n"
        "}\n";
    ANGLE_GL_PROGRAM(program, vs, fs);
    glUseProgram(program);
    GLuint mColorUniformLoc = glGetUniformLocation(program, "uCol");

    initializeFBOs(1, true, false);
    glEnable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);

    // Set clear values and clear the whole framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO);
    glClearColor(0, 0, 0, 0);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Update stencil test to always replace the stencil value with 0xFF.
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilFunc(GL_ALWAYS, 0xFF, 0);

    // Draw a quad which covers the whole texture.
    glUniform3f(mColorUniformLoc, 1.0f, 0.0f, 0.0f);
    drawQuad(program, "vPos", 0.0f, 1.0f, true);

    // Switch to multiview framebuffer and clear portions of the texture.
    glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);
    glViewport(0, 0, 1, 2);
    glScissor(0, 0, 1, 2);
    glEnable(GL_SCISSOR_TEST);
    glClear(GL_STENCIL_BUFFER_BIT);

    // Draw a fullscreen quad, but adjust the stencil function so that only the cleared regions pass
    // the test.
    glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO);
    glViewport(0, 0, 4, 2);
    glScissor(0, 0, 4, 2);
    glStencilFunc(GL_EQUAL, 0x00, 0xFF);
    glUniform3f(mColorUniformLoc, 0.0f, 1.0f, 0.0f);
    drawQuad(program, "vPos", 0.0f, 1.0f, true);

    checkAlternatingColumnsOfRedAndGreenInFBO();
}

// Test that glClearBufferf clears the color buffer of each view.
TEST_P(FramebufferMultiviewSideBySideClearTest, ClearBufferF)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    initializeFBOs(2, false, false);

    // Clear the contents of the texture.
    glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Bind and specify viewport/scissor dimensions for each view.
    glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);
    glViewport(0, 0, 1, 2);
    glScissor(0, 0, 1, 2);
    glEnable(GL_SCISSOR_TEST);

    float kClearValues[4] = {0.f, 1.f, 0.f, 1.f};
    glClearBufferfv(GL_COLOR, 1, kClearValues);

    glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO);

    // Check that glClearBufferfv has not modified the 0th color attachment.
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            EXPECT_PIXEL_COLOR_EQ(i, j, GLColor::red);
        }
    }

    // Check that glClearBufferfv has correctly modified the 1th color attachment.
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    checkAlternatingColumnsOfRedAndGreenInFBO();
}

// Test that glClear clears the contents of the color buffer for only the attached layers to a
// layered FBO.
TEST_P(FramebufferMultiviewLayeredClearTest, ColorBufferClear)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    initializeFBOs(1, 1, 4, 1, 2, 1, false, false);

    // Bind and specify viewport/scissor dimensions for each view.
    glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);

    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(0, GL_COLOR_ATTACHMENT0));
    EXPECT_EQ(GLColor::green, getLayerColor(1, GL_COLOR_ATTACHMENT0));
    EXPECT_EQ(GLColor::green, getLayerColor(2, GL_COLOR_ATTACHMENT0));
    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(3, GL_COLOR_ATTACHMENT0));
}

// Test that glClearBufferfv can be used to clear individual color buffers of a layered FBO.
TEST_P(FramebufferMultiviewLayeredClearTest, ClearIndividualColorBuffer)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    initializeFBOs(1, 1, 4, 1, 2, 2, false, false);

    for (int i = 0; i < 2; ++i)
    {
        for (int layer = 0; layer < 4; ++layer)
        {
            GLenum colorAttachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
            EXPECT_EQ(GLColor::transparentBlack, getLayerColor(layer, colorAttachment));
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);

    float clearValues0[4] = {0.f, 0.f, 1.f, 1.f};
    glClearBufferfv(GL_COLOR, 0, clearValues0);

    float clearValues1[4] = {0.f, 1.f, 0.f, 1.f};
    glClearBufferfv(GL_COLOR, 1, clearValues1);

    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(0, GL_COLOR_ATTACHMENT0));
    EXPECT_EQ(GLColor::blue, getLayerColor(1, GL_COLOR_ATTACHMENT0));
    EXPECT_EQ(GLColor::blue, getLayerColor(2, GL_COLOR_ATTACHMENT0));
    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(3, GL_COLOR_ATTACHMENT0));

    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(0, GL_COLOR_ATTACHMENT1));
    EXPECT_EQ(GLColor::green, getLayerColor(1, GL_COLOR_ATTACHMENT1));
    EXPECT_EQ(GLColor::green, getLayerColor(2, GL_COLOR_ATTACHMENT1));
    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(3, GL_COLOR_ATTACHMENT1));
}

// Test that glClearBufferfi clears the contents of the stencil buffer for only the attached layers
// to a layered FBO.
TEST_P(FramebufferMultiviewLayeredClearTest, ClearBufferfi)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    // Create program to draw a quad.
    const std::string &vs =
        "#version 300 es\n"
        "in vec3 vPos;\n"
        "void main(){\n"
        "   gl_Position = vec4(vPos, 1.);\n"
        "}\n";
    const std::string &fs =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform vec3 uCol;\n"
        "out vec4 col;\n"
        "void main(){\n"
        "   col = vec4(uCol,1.);\n"
        "}\n";
    ANGLE_GL_PROGRAM(program, vs, fs);
    glUseProgram(program);
    GLuint mColorUniformLoc = glGetUniformLocation(program, "uCol");

    initializeFBOs(1, 1, 4, 1, 2, 1, true, false);
    glEnable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);

    // Set clear values.
    glClearColor(1, 0, 0, 1);
    glClearStencil(0xFF);

    // Clear the color and stencil buffers of each layer.
    for (size_t i = 0u; i < mNonMultiviewFBO.size(); ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO[i]);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    // Switch to multiview framebuffer and clear portions of the texture.
    glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);
    glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.0f, 0);

    // Draw a fullscreen quad, but adjust the stencil function so that only the cleared regions pass
    // the test.
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilFunc(GL_EQUAL, 0x00, 0xFF);
    for (size_t i = 0u; i < mNonMultiviewFBO.size(); ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO[i]);
        glUniform3f(mColorUniformLoc, 0.0f, 1.0f, 0.0f);
        drawQuad(program, "vPos", 0.0f, 1.0f, true);
    }
    EXPECT_EQ(GLColor::red, getLayerColor(0, GL_COLOR_ATTACHMENT0));
    EXPECT_EQ(GLColor::green, getLayerColor(1, GL_COLOR_ATTACHMENT0));
    EXPECT_EQ(GLColor::green, getLayerColor(2, GL_COLOR_ATTACHMENT0));
    EXPECT_EQ(GLColor::red, getLayerColor(3, GL_COLOR_ATTACHMENT0));
}

// Test that glClear does not clear the content of a detached texture.
TEST_P(FramebufferMultiviewLayeredClearTest, UnmodifiedDetachedTexture)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    initializeFBOs(1, 1, 4, 1, 2, 2, false, false);

    // Clear all attachments.
    glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    for (int i = 0; i < 2; ++i)
    {
        GLenum colorAttachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
        EXPECT_EQ(GLColor::transparentBlack, getLayerColor(0, colorAttachment));
        EXPECT_EQ(GLColor::green, getLayerColor(1, colorAttachment));
        EXPECT_EQ(GLColor::green, getLayerColor(2, colorAttachment));
        EXPECT_EQ(GLColor::transparentBlack, getLayerColor(3, colorAttachment));
    }

    // Detach and clear again.
    glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);
    glFramebufferTextureMultiviewLayeredANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, 0, 0, 1, 2);
    glClearColor(1, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Check that color attachment 0 is modified.
    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(0, GL_COLOR_ATTACHMENT0));
    EXPECT_EQ(GLColor::yellow, getLayerColor(1, GL_COLOR_ATTACHMENT0));
    EXPECT_EQ(GLColor::yellow, getLayerColor(2, GL_COLOR_ATTACHMENT0));
    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(3, GL_COLOR_ATTACHMENT0));

    // Check that color attachment 1 is unmodified.
    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(0, GL_COLOR_ATTACHMENT1));
    EXPECT_EQ(GLColor::green, getLayerColor(1, GL_COLOR_ATTACHMENT1));
    EXPECT_EQ(GLColor::green, getLayerColor(2, GL_COLOR_ATTACHMENT1));
    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(3, GL_COLOR_ATTACHMENT1));
}

// Test that glClear clears only the contents within the scissor rectangle of the attached layers.
TEST_P(FramebufferMultiviewLayeredClearTest, ScissoredClear)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    initializeFBOs(2, 1, 4, 1, 2, 1, false, false);

    // Bind and specify viewport/scissor dimensions for each view.
    glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);

    glEnable(GL_SCISSOR_TEST);
    glScissor(1, 0, 1, 1);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(0, GL_COLOR_ATTACHMENT0, 0, 0));
    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(0, GL_COLOR_ATTACHMENT0, 1, 0));

    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(1, GL_COLOR_ATTACHMENT0, 0, 0));
    EXPECT_EQ(GLColor::green, getLayerColor(1, GL_COLOR_ATTACHMENT0, 1, 0));

    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(2, GL_COLOR_ATTACHMENT0, 0, 0));
    EXPECT_EQ(GLColor::green, getLayerColor(2, GL_COLOR_ATTACHMENT0, 1, 0));

    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(3, GL_COLOR_ATTACHMENT0, 0, 0));
    EXPECT_EQ(GLColor::transparentBlack, getLayerColor(3, GL_COLOR_ATTACHMENT0, 1, 0));
}

// Test that glClearBufferfi clears the contents of the stencil buffer for only the attached layers
// to a layered FBO.
TEST_P(FramebufferMultiviewLayeredClearTest, ScissoredClearBufferfi)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    // Create program to draw a quad.
    const std::string &vs =
        "#version 300 es\n"
        "in vec3 vPos;\n"
        "void main(){\n"
        "   gl_Position = vec4(vPos, 1.);\n"
        "}\n";
    const std::string &fs =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform vec3 uCol;\n"
        "out vec4 col;\n"
        "void main(){\n"
        "   col = vec4(uCol,1.);\n"
        "}\n";
    ANGLE_GL_PROGRAM(program, vs, fs);
    glUseProgram(program);
    GLuint mColorUniformLoc = glGetUniformLocation(program, "uCol");

    initializeFBOs(1, 2, 4, 1, 2, 1, true, false);
    glEnable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);

    // Set clear values.
    glClearColor(1, 0, 0, 1);
    glClearStencil(0xFF);

    // Clear the color and stencil buffers of each layer.
    for (size_t i = 0u; i < mNonMultiviewFBO.size(); ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO[i]);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    // Switch to multiview framebuffer and clear portions of the texture.
    glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 1, 1);
    glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.0f, 0);
    glDisable(GL_SCISSOR_TEST);

    // Draw a fullscreen quad, but adjust the stencil function so that only the cleared regions pass
    // the test.
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilFunc(GL_EQUAL, 0x00, 0xFF);
    glUniform3f(mColorUniformLoc, 0.0f, 1.0f, 0.0f);
    for (size_t i = 0u; i < mNonMultiviewFBO.size(); ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mNonMultiviewFBO[i]);
        drawQuad(program, "vPos", 0.0f, 1.0f, true);
    }
    EXPECT_EQ(GLColor::red, getLayerColor(0, GL_COLOR_ATTACHMENT0, 0, 0));
    EXPECT_EQ(GLColor::red, getLayerColor(0, GL_COLOR_ATTACHMENT0, 0, 1));
    EXPECT_EQ(GLColor::green, getLayerColor(1, GL_COLOR_ATTACHMENT0, 0, 0));
    EXPECT_EQ(GLColor::red, getLayerColor(1, GL_COLOR_ATTACHMENT0, 0, 1));
    EXPECT_EQ(GLColor::green, getLayerColor(2, GL_COLOR_ATTACHMENT0, 0, 0));
    EXPECT_EQ(GLColor::red, getLayerColor(2, GL_COLOR_ATTACHMENT0, 0, 1));
    EXPECT_EQ(GLColor::red, getLayerColor(3, GL_COLOR_ATTACHMENT0, 0, 0));
    EXPECT_EQ(GLColor::red, getLayerColor(3, GL_COLOR_ATTACHMENT0, 0, 1));
}

// Test that detaching an attachment does not generate an error whenever the multi-view related
// arguments are invalid.
TEST_P(FramebufferMultiviewTest, InvalidMultiviewArgumentsOnDetach)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Invalid base view index.
    glFramebufferTextureMultiviewLayeredANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0, -1, 1);
    EXPECT_GL_NO_ERROR();

    // Invalid number of views.
    glFramebufferTextureMultiviewLayeredANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0, 0, 0);
    EXPECT_GL_NO_ERROR();

    // Invalid number of views.
    const GLint kValidViewportOffsets[2] = {0, 0};
    glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0, 0,
                                                 kValidViewportOffsets);
    EXPECT_GL_NO_ERROR();

    // Invalid viewport offsets.
    const GLint kInvalidViewportOffsets[2] = {-1, -1};
    glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0, 1,
                                                 kInvalidViewportOffsets);
    EXPECT_GL_NO_ERROR();
}

// Test that glClear clears the contents of the color buffer whenever all layers of a 2D texture
// array are attached. The test is added because a special fast code path is used for this case.
TEST_P(FramebufferMultiviewLayeredClearTest, ColorBufferClearAllLayersAttached)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());

    initializeFBOs(1, 1, 2, 0, 2, 1, false, false);

    glBindFramebuffer(GL_FRAMEBUFFER, mMultiviewFBO);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    EXPECT_EQ(GLColor::green, getLayerColor(0, GL_COLOR_ATTACHMENT0));
    EXPECT_EQ(GLColor::green, getLayerColor(1, GL_COLOR_ATTACHMENT0));
}

ANGLE_INSTANTIATE_TEST(FramebufferMultiviewTest, VertexShaderOpenGL(3, 0), GeomShaderD3D11(3, 0));
ANGLE_INSTANTIATE_TEST(FramebufferMultiviewSideBySideClearTest,
                       VertexShaderOpenGL(3, 0),
                       GeomShaderD3D11(3, 0));
ANGLE_INSTANTIATE_TEST(FramebufferMultiviewLayeredClearTest,
                       VertexShaderOpenGL(3, 0),
                       GeomShaderD3D11(3, 0));
