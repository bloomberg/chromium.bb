//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferVk.h:
//    Defines the class interface for FramebufferVk, implementing FramebufferImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_FRAMEBUFFERVK_H_
#define LIBANGLE_RENDERER_VULKAN_FRAMEBUFFERVK_H_

#include "libANGLE/renderer/FramebufferImpl.h"
#include "libANGLE/renderer/RenderTargetCache.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/CommandGraph.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"

namespace rx
{
class RendererVk;
class RenderTargetVk;
class WindowSurfaceVk;

class FramebufferVk : public FramebufferImpl
{
  public:
    // Factory methods so we don't have to use constructors with overloads.
    static FramebufferVk *CreateUserFBO(RendererVk *renderer, const gl::FramebufferState &state);

    // The passed-in SurfaceVK must be destroyed after this FBO is destroyed. Our Surface code is
    // ref-counted on the number of 'current' contexts, so we shouldn't get any dangling surface
    // references. See Surface::setIsCurrent(bool).
    static FramebufferVk *CreateDefaultFBO(RendererVk *renderer,
                                           const gl::FramebufferState &state,
                                           WindowSurfaceVk *backbuffer);

    ~FramebufferVk() override;
    void destroy(const gl::Context *context) override;

    gl::Error discard(const gl::Context *context, size_t count, const GLenum *attachments) override;
    gl::Error invalidate(const gl::Context *context,
                         size_t count,
                         const GLenum *attachments) override;
    gl::Error invalidateSub(const gl::Context *context,
                            size_t count,
                            const GLenum *attachments,
                            const gl::Rectangle &area) override;

    gl::Error clear(const gl::Context *context, GLbitfield mask) override;
    gl::Error clearBufferfv(const gl::Context *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLfloat *values) override;
    gl::Error clearBufferuiv(const gl::Context *context,
                             GLenum buffer,
                             GLint drawbuffer,
                             const GLuint *values) override;
    gl::Error clearBufferiv(const gl::Context *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLint *values) override;
    gl::Error clearBufferfi(const gl::Context *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            GLfloat depth,
                            GLint stencil) override;

    GLenum getImplementationColorReadFormat(const gl::Context *context) const override;
    GLenum getImplementationColorReadType(const gl::Context *context) const override;
    gl::Error readPixels(const gl::Context *context,
                         const gl::Rectangle &area,
                         GLenum format,
                         GLenum type,
                         void *pixels) override;

    gl::Error blit(const gl::Context *context,
                   const gl::Rectangle &sourceArea,
                   const gl::Rectangle &destArea,
                   GLbitfield mask,
                   GLenum filter) override;

    bool checkStatus(const gl::Context *context) const override;

    angle::Result syncState(const gl::Context *context,
                            const gl::Framebuffer::DirtyBits &dirtyBits) override;

    gl::Error getSamplePosition(const gl::Context *context,
                                size_t index,
                                GLfloat *xy) const override;
    RenderTargetVk *getDepthStencilRenderTarget() const;
    const vk::RenderPassDesc &getRenderPassDesc();

    // Internal helper function for readPixels operations.
    angle::Result readPixelsImpl(ContextVk *contextVk,
                                 const gl::Rectangle &area,
                                 const PackPixelsParams &packPixelsParams,
                                 VkImageAspectFlagBits copyAspectFlags,
                                 RenderTargetVk *renderTarget,
                                 void *pixels);

    const gl::Extents &getReadImageExtents() const;

    gl::DrawBufferMask getEmulatedAlphaAttachmentMask();
    RenderTargetVk *getColorReadRenderTarget() const;

    // This will clear the current write operation if it is complete.
    bool appendToStartedRenderPass(RendererVk *renderer, vk::CommandBuffer **commandBufferOut)
    {
        return mFramebuffer.appendToStartedRenderPass(renderer, commandBufferOut);
    }

    vk::FramebufferHelper *getFramebuffer() { return &mFramebuffer; }

    angle::Result startNewRenderPass(ContextVk *context, vk::CommandBuffer **commandBufferOut);

  private:
    FramebufferVk(RendererVk *renderer,
                  const gl::FramebufferState &state,
                  WindowSurfaceVk *backbuffer);

    // Helper for appendToStarted/else startNewRenderPass.
    angle::Result getCommandBufferForDraw(ContextVk *contextVk,
                                          vk::CommandBuffer **commandBufferOut,
                                          vk::RecordingMode *modeOut);

    // The 'in' rectangles must be clipped to the scissor and FBO. The clipping is done in 'blit'.
    angle::Result blitWithCommand(ContextVk *contextVk,
                                  const gl::Rectangle &readRectIn,
                                  const gl::Rectangle &drawRectIn,
                                  RenderTargetVk *readRenderTarget,
                                  RenderTargetVk *drawRenderTarget,
                                  GLenum filter,
                                  bool colorBlit,
                                  bool depthBlit,
                                  bool stencilBlit,
                                  bool flipSource,
                                  bool flipDest);

    // Note that 'copyArea' must be clipped to the scissor and FBO. The clipping is done in 'blit'.
    angle::Result blitWithCopy(ContextVk *contextVk,
                               const gl::Rectangle &copyArea,
                               RenderTargetVk *readRenderTarget,
                               RenderTargetVk *drawRenderTarget,
                               bool blitDepthBuffer,
                               bool blitStencilBuffer);

    angle::Result blitWithReadback(ContextVk *contextVk,
                                   const gl::Rectangle &copyArea,
                                   VkImageAspectFlagBits aspect,
                                   RenderTargetVk *readRenderTarget,
                                   RenderTargetVk *drawRenderTarget);

    angle::Result getFramebuffer(ContextVk *contextVk, vk::Framebuffer **framebufferOut);

    angle::Result clearWithClearAttachments(ContextVk *contextVk,
                                            bool clearColor,
                                            bool clearDepth,
                                            bool clearStencil);
    angle::Result clearWithDraw(ContextVk *contextVk, VkColorComponentFlags colorMaskFlags);
    void updateActiveColorMasks(size_t colorIndex, bool r, bool g, bool b, bool a);

    WindowSurfaceVk *mBackbuffer;

    Optional<vk::RenderPassDesc> mRenderPassDesc;
    vk::FramebufferHelper mFramebuffer;
    RenderTargetCache<RenderTargetVk> mRenderTargetCache;

    // These two variables are used to quickly compute if we need to do a masked clear. If a color
    // channel is masked out, we check against the Framebuffer Attachments (RenderTargets) to see
    // if the masked out channel is present in any of the attachments.
    VkColorComponentFlags mActiveColorComponents;
    gl::DrawBufferMask mActiveColorComponentMasksForClear[4];
    vk::DynamicBuffer mReadPixelBuffer;
    vk::DynamicBuffer mBlitPixelBuffer;

    // When we draw to the framebuffer, and the real format has an alpha channel but the format of
    // the framebuffer does not, we need to mask out the alpha channel. This DrawBufferMask will
    // contain the mask to apply to the alpha channel when drawing.
    gl::DrawBufferMask mEmulatedAlphaAttachmentMask;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_FRAMEBUFFERVK_H_
